import os
import re
import torch
import numpy as np
from torch_geometric.datasets import Planetoid

# ================= 动态路径计算 =================
# 获取当前脚本本身所在的绝对目录 (.../Sanger-Sim/tests)
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
# 推导项目根目录 (.../Sanger-Sim)
PROJECT_ROOT = os.path.dirname(SCRIPT_DIR)

def load_hardware_config():
    # 现在会去正确的上级目录找 config 和 inc
    config_paths = [
        os.path.join(PROJECT_ROOT, "config", "config.h"),
        os.path.join(PROJECT_ROOT, "inc", "config.h")
    ]
    
    config_file = None
    for path in config_paths:
        if os.path.exists(path):
            config_file = path
            break
            
    if not config_file:
        raise FileNotFoundError(f"找不到 config.h 文件！请检查 {PROJECT_ROOT} 下是否有 config/ 或 inc/ 目录。")
        
    with open(config_file, "r", encoding="utf-8") as f:
        content = f.read()
        
    def extract_val(var_names, default):
        for name in var_names:
            match = re.search(fr"(?:constexpr|const)\s+(?:int|size_t)\s+{name}\s*=\s*(\d+)", content)
            if match: return int(match.group(1))
            match = re.search(fr"#define\s+{name}\s+(\d+)", content)
            if match: return int(match.group(1))
        return default

    return {
        "M": extract_val(["DEFAULT_TILE_M", "TILE_M"], 16),
        "N": extract_val(["DEFAULT_TILE_N", "TILE_N"], 32),
        "D": extract_val(["DEFAULT_HEAD_DIM", "HEAD_DIM"], 32)
    }

print("=== 阶段 1/3: 加载硬件配置 ===")
hw_cfg = load_hardware_config()
TILE_M, TILE_N, HEAD_DIM = hw_cfg["M"], hw_cfg["N"], hw_cfg["D"]
print(f"[Success] M={TILE_M}, N={TILE_N}, D={HEAD_DIM}")

# 强制将生成的小块存放在 tests/cora_tiles 目录下
OUTPUT_DIR = os.path.join(SCRIPT_DIR, "cora_tiles")
if not os.path.exists(OUTPUT_DIR):
    os.makedirs(OUTPUT_DIR)

# 将下载的数据也统一管理在 tests/data 目录下
DATA_DIR = r"C:\Users\Lenovo\data"

print("\n=== 阶段 2/3: 加载 Cora 数据集 ===")
dataset = Planetoid(root=DATA_DIR, name='Cora')
data = dataset[0]
num_nodes = data.num_nodes

print("\n=== 阶段 3/3: 仿真特征生成与稀疏切块 ===")
torch.manual_seed(42)
Q_full = torch.randn(num_nodes, HEAD_DIM)
K_full = torch.randn(num_nodes, HEAD_DIM)
V_full = torch.randn(num_nodes, HEAD_DIM)

adj = torch.zeros((num_nodes, num_nodes))
edge_index = data.edge_index
adj[edge_index[0], edge_index[1]] = 1.0

num_blocks_m = int(np.ceil(num_nodes / TILE_M))
num_blocks_n = int(np.ceil(num_nodes / TILE_N))
valid_tiles_count = 0

for r in range(num_blocks_m):
    for c in range(num_blocks_n):
        row_start = r * TILE_M; row_end = min((r + 1) * TILE_M, num_nodes)
        col_start = c * TILE_N; col_end = min((c + 1) * TILE_N, num_nodes)
        
        if adj[row_start:row_end, col_start:col_end].sum() > 0:
            q_tile = torch.zeros(TILE_M, HEAD_DIM); k_tile = torch.zeros(TILE_N, HEAD_DIM); v_tile = torch.zeros(TILE_N, HEAD_DIM)
            
            q_len, k_len = row_end - row_start, col_end - col_start
            q_tile[:q_len, :] = Q_full[row_start:row_end, :]
            k_tile[:k_len, :] = K_full[col_start:col_end, :]
            v_tile[:k_len, :] = V_full[col_start:col_end, :]
            
            np.savetxt(os.path.join(OUTPUT_DIR, f"q_{valid_tiles_count}.txt"), q_tile.numpy().flatten(), fmt='%.6f')
            np.savetxt(os.path.join(OUTPUT_DIR, f"k_{valid_tiles_count}.txt"), k_tile.numpy().flatten(), fmt='%.6f')
            np.savetxt(os.path.join(OUTPUT_DIR, f"v_{valid_tiles_count}.txt"), v_tile.numpy().flatten(), fmt='%.6f')
            valid_tiles_count += 1

with open(os.path.join(OUTPUT_DIR, "meta.txt"), "w") as f:
    f.write(str(valid_tiles_count))

print(f"\n[Done] 预处理完成！保存在 {OUTPUT_DIR}")