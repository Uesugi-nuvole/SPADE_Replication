import os
import re
import torch
import numpy as np
from torch_geometric.datasets import Planetoid

# ================= 动态解析 C++ Config =================
def load_hardware_config(config_path):
    if not os.path.exists(config_path):
        print(f"[Warning] 找不到 {config_path}，将按默认值执行。")
        return {"M": 256, "K": 1024, "D": 32} # SPADE 常用大块
        
    with open(config_path, 'r', encoding='utf-8') as f:
        content = f.read()

    def extract_val(names, default):
        for name in names:
            # 匹配 constexpr int X = 123; 这样的 C++ 语法
            match = re.search(fr"constexpr\s+int\s+{name}\s*=\s*(\d+);", content)
            if match:
                return int(match.group(1))
        return default

    # SPADE 图切分映射：
    # C++ DEFAULT_TILE_M -> 行分界粒度 (Tile M)
    # C++ DEFAULT_TILE_N -> 邻接阵列分界粒度，对应图的节点特征行 (Tile K)
    # C++ DEFAULT_HEAD_DIM -> 特征向量维度 (D)
    return {
        "M": extract_val(["DEFAULT_TILE_M"], 256),
        "K": extract_val(["DEFAULT_TILE_N"], 1024), 
        "D": extract_val(["DEFAULT_HEAD_DIM"], 32)
    }

# ================= 路径与参数设定 =================
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
#  config.h 相对当前脚本在上一级的 configs/ 文件夹里 
CONFIG_PATH = os.path.abspath(os.path.join(SCRIPT_DIR, "../configs/config.h"))

DATASET_NAME = 'Cora'
DATA_DIR = "/tmp/Cora"  # 或者用你之前的 r"C:\Users\Lenovo\data"
OUTPUT_DIR = os.path.join(SCRIPT_DIR, '../data/spade_tiles')

def main():
    print("=== 阶段 1/3: 动态加载硬件全局配置 ===")
    hw_cfg = load_hardware_config(CONFIG_PATH)
    TILE_M, TILE_K, HEAD_DIM = hw_cfg["M"], hw_cfg["K"], hw_cfg["D"]
    print(f"[Success] 读取到硬件配置 -> TILE_M={TILE_M}, TILE_K={TILE_K}, HEAD_DIM={HEAD_DIM}")

    print(f"\n=== 阶段 2/3: Loading {DATASET_NAME} dataset ===")
    dataset = Planetoid(root=DATA_DIR, name=DATASET_NAME)
    data = dataset[0]

    num_nodes = data.num_nodes
    edges = data.edge_index

    # 构建稀疏邻接矩阵
    adj = torch.zeros((num_nodes, num_nodes), dtype=torch.float32)
    adj[edges[0], edges[1]] = 1.0

    os.makedirs(OUTPUT_DIR, exist_ok=True)
    meta_file = open(os.path.join(OUTPUT_DIR, 'metadata.txt'), 'w')
    
    # 写入 Metadata 表头
    meta_file.write("tile_id row_start row_num col_start col_num nnz bypass_cache_l bypass_cache_r use_victim_cache\n")

    num_blocks_m = int(np.ceil(num_nodes / TILE_M))
    num_blocks_k = int(np.ceil(num_nodes / TILE_K))

    tile_id = 0
    valid_tiles = 0

    print(f"\n=== 阶段 3/3: 按 ({TILE_M}x{TILE_K}) 分块并生成 SPADE 调度字典 ===")
    
    for i in range(num_blocks_m):
        for j in range(num_blocks_k):
            row_start = i * TILE_M
            row_end = min((i + 1) * TILE_M, num_nodes)
            col_start = j * TILE_K
            col_end = min((j + 1) * TILE_K, num_nodes)

            sub_block = adj[row_start:row_end, col_start:col_end]
            nnz = int(sub_block.sum().item())

            # 核心：只处理非零元素的块
            if nnz > 0:
                # 左矩阵 (图拓扑 A)：强制 Bypass L2
                bypass_cache_l = 1 
                
                # 右矩阵 (特征 X)：根据当前块稀疏度判定
                if nnz < (TILE_M * TILE_K * 0.05): 
                    bypass_cache_r = 1
                    use_victim = 0
                else:
                    bypass_cache_r = 0
                    use_victim = 1

                meta_file.write(f"{tile_id} {row_start} {row_end - row_start} {col_start} {col_end - col_start} {nnz} {bypass_cache_l} {bypass_cache_r} {use_victim}\n")

                indices = torch.nonzero(sub_block, as_tuple=False)
                tile_data_path = os.path.join(OUTPUT_DIR, f"tile_{tile_id}.txt")
                with open(tile_data_path, 'w') as f:
                    for idx in indices:
                        r, c = idx[0].item(), idx[1].item()
                        val = sub_block[r, c].item()
                        f.write(f"{r} {c} {val}\n")

                valid_tiles += 1
            
            tile_id += 1

    meta_file.close()
    print(f"\n[Done] 提取完成! 覆盖格点总数: {tile_id}, 包含非零元素的有效块: {valid_tiles}")
    print(f"数据存放路径: {OUTPUT_DIR}")

if __name__ == "__main__":
    main()