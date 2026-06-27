import os
import torch
import numpy as np
from torch_geometric.datasets import Planetoid

# ================= 配置参数 =================
DATASET_NAME = 'Cora'
TILE_M = 16  # 行分块大小
TILE_K = 64  # 列分块大小 (对应图邻接矩阵的列)
OUTPUT_DIR = '../data/spade_tiles'

def main():
    print(f"Loading {DATASET_NAME} dataset...")
    dataset = Planetoid(root='/tmp/Cora', name=DATASET_NAME)
    data = dataset[0]

    num_nodes = data.num_nodes
    edges = data.edge_index

    # 1. 构建稀疏邻接矩阵 (Adjacency Matrix)
    # 此处我们只关注图拓扑(A矩阵)，模拟 SpMM (A * X) 中 A 的切分
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

    print(f"Partitioning Adjacency Matrix ({num_nodes}x{num_nodes}) into {TILE_M}x{TILE_K} tiles...")

    # 2. 遍历所有划分块 (Tiling)
    for i in range(num_blocks_m):
        for j in range(num_blocks_k):
            row_start = i * TILE_M
            row_end = min((i + 1) * TILE_M, num_nodes)
            col_start = j * TILE_K
            col_end = min((j + 1) * TILE_K, num_nodes)

            # 提取子块
            sub_block = adj[row_start:row_end, col_start:col_end]
            nnz = int(sub_block.sum().item())

            # 核心：只处理非零元素的块 (Block-level Sparsity)
            if nnz > 0:
                # -------------------------------------------------------------
                # SPADE 核心逻辑：生成基于 Tile 的 Cache 策略 (启发式)
                # -------------------------------------------------------------
                # 左矩阵 (A矩阵，即图拓扑)：由于是流式读取且极少复用，强制 Bypass L2 Cache! (论文核心优化点)
                bypass_cache_l = 1 
                
                # 右矩阵 (X特征矩阵)：
                # 如果这个块非常稀疏 (nnz很少)，说明右侧的特征行只会被读取寥寥几次，Bypass L2 避免污染
                # 如果 nnz 较多，启用 L2 Cache。由于是 R-Matrix，尝试使用 Victim Cache 捕获时间重用。
                if nnz < (TILE_M * TILE_K * 0.05): # 稀疏度高于 95%
                    bypass_cache_r = 1
                    use_victim = 0
                else:
                    bypass_cache_r = 0
                    use_victim = 1

                # 记录 Metadata
                meta_file.write(f"{tile_id} {row_start} {row_end - row_start} {col_start} {col_end - col_start} {nnz} {bypass_cache_l} {bypass_cache_r} {use_victim}\n")

                # 3. 提取非零元素的坐标和值 (COO 格式)，只存有效数据
                indices = torch.nonzero(sub_block, as_tuple=False)
                
                # 写入具体的 Tile 数据文件
                tile_data_path = os.path.join(OUTPUT_DIR, f"tile_{tile_id}.txt")
                with open(tile_data_path, 'w') as f:
                    for idx in indices:
                        r, c = idx[0].item(), idx[1].item()
                        val = sub_block[r, c].item()
                        f.write(f"{r} {c} {val}\n")

                valid_tiles += 1
            
            tile_id += 1

    meta_file.close()
    print(f"Extraction complete! Total grid tiles: {tile_id}, Valid non-zero tiles generated: {valid_tiles}")
    print(f"Tile metadata and COO data saved to {OUTPUT_DIR}")

if __name__ == "__main__":
    main()