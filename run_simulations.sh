#!/bin/bash

# 你的可执行程序路径。请确保这个路径是正确的。
EXECUTABLE="./GNN" 

# 定义输出文件存放的目录。如果不存在，脚本会创建它。
OUTPUT_DIR="./output/result"
mkdir -p "$OUTPUT_DIR" # 确保输出目录存在

# 定义一个单独的目录来存放 run.log 文件
LOG_DIR="./output/logs"
mkdir -p "$LOG_DIR" # 确保日志目录存在

# --- 数据集参数配置 ---
# 格式: dataset_name total_row_round total_slice_num total_round in_feature_length row_num
declare -A DATASET_CONFIGS
DATASET_CONFIGS["cora_L1"]="./sgcn_graphs/data/sgcn_Cora_vertex_ \
./sgcn_graphs/data/sgcn_Cora_edge_ ./sgcn_graphs/data/sgcn_Cora_num.txt \
1 1 13264 2708 32 16 2 1"

DATASET_CONFIGS["cora_L2"]="./sgcn_graphs/data/sgcn_Cora_vertex_ \
./sgcn_graphs/data/sgcn_Cora_edge_ ./sgcn_graphs/data/sgcn_Cora_num.txt \
1 1 13264 2708 16 16 1 1"


DATASET_CONFIGS["pubmed_L1"]="./sgcn_graphs/data/sgcn_PubMed_vertex_ \
./sgcn_graphs/data/sgcn_PubMed_edge_ ./sgcn_graphs/data/sgcn_PubMed_num.txt \
2 4 108365 19717 32 16 2 1"
DATASET_CONFIGS["pubmed_L2"]="./sgcn_graphs/data/sgcn_PubMed_vertex_ \
./sgcn_graphs/data/sgcn_PubMed_edge_ ./sgcn_graphs/data/sgcn_PubMed_num.txt \
2 4 108365 19717 16 16 1 1"

DATASET_CONFIGS["flickr_L1"]="./sgcn_graphs/sgcn_Flickr_vertex_ \
./sgcn_graphs/sgcn_Flickr_edge_ ./sgcn_graphs/sgcn_Flickr_num.txt \
6 36 13954819 716847 32 64 2 4"
DATASET_CONFIGS["flickr_L2"]="./sgcn_graphs/sgcn_Flickr_vertex_ \
./sgcn_graphs/sgcn_Flickr_edge_ ./sgcn_graphs/sgcn_Flickr_num.txt \
6 36 13954819 716847 32 16 2 1"

DATASET_CONFIGS["Reddit_L1"]="./sgcn_graphs/sgcn_Reddit_vertex_ \
./sgcn_graphs/sgcn_Reddit_edge_ ./sgcn_graphs/sgcn_Reddit_num.txt \
15 225 114848857 232965 32 64 2 4"

DATASET_CONFIGS["Yelp_L1"]="./sgcn_graphs/sgcn_Yelp_vertex_ \
./sgcn_graphs/sgcn_Yelp_edge_ ./sgcn_graphs/sgcn_Yelp_num.txt \
44 1936 13954819 716847 32 64 2 4"


DATASET_CONFIGS["proteins_L1"]="./sgcn_graphs/data/sgcn_proteins_vertex_ \
./sgcn_graphs/data/sgcn_proteins_edge_ ./sgcn_graphs/data/sgcn_proteins_num.txt \
9 81 79122504 132534 16 64 1 4"
DATASET_CONFIGS["proteins_L2"]="./sgcn_graphs/data/sgcn_proteins_vertex_ \
./sgcn_graphs/data/sgcn_proteins_edge_ ./sgcn_graphs/data/sgcn_proteins_num.txt \
9 81 79122504 132534 16 16 1 1"

DATASET_CONFIGS["Pokec_L1"]="./sgcn_graphs/sgcn_Pokec/sgcn_Pokec_vertex_ \
./sgcn_graphs/sgcn_Pokec/sgcn_Pokec_edge_ ./sgcn_graphs/sgcn_Pokec/shape_counts_Pokec.txt \
100 10000 30622564 1632803 32 64 2 4"
DATASET_CONFIGS["Pokec_L2"]="./sgcn_graphs/sgcn_Pokec/sgcn_Pokec_vertex_ \
./sgcn_graphs/sgcn_Pokec/sgcn_Pokec_edge_ ./sgcn_graphs/sgcn_Pokec/shape_counts_Pokec.txt \
100 10000 30622564 1632803 32 16 2 1"
# --- 脚本开始 ---
echo "--- 仿真开始 ---"
echo "所有仿真结果将保存到: $OUTPUT_DIR"
echo "所有运行日志将保存到: $LOG_DIR"
echo ""

# 定义所有要运行的 dataset_key 列表
SIMULATION_KEYS=( \
  "cora_L2" \
#    "cora_L2"  \
  # "pubmed_L1"  "pubmed_L2" \
  #  "flickr_L1" \ 
   #"flickr_L2" \
   # "Reddit_L1"  \
 #  "proteins_L1" 
  #  "proteins_L2" \
  #  "Yelp_L1"  \
#   "Pokec_L1" \
#   "Pokec_L2"
)


# --- 定义合并输出文件 ---
CORA_COMBINED_OUTPUT_FILE="${OUTPUT_DIR}/cora_combined_simulation_output.txt"
PUBMED_COMBINED_OUTPUT_FILE="${OUTPUT_DIR}/pubmed_combined_simulation_output.txt"
FLICKR_COMBINED_OUTPUT_FILE="${OUTPUT_DIR}/flickr_combined_simulation_output.txt"
REDDIT_COMBINED_OUTPUT_FILE="${OUTPUT_DIR}/Reddit_combined_simulation_output.txt" 
YELP_COMBINED_OUTPUT_FILE="${OUTPUT_DIR}/Yelp_combined_simulation_output.txt"     
PRO_COMBINED_OUTPUT_FILE="${OUTPUT_DIR}/proteins_combined_simulation_output.txt"
POKEC_COMBINED_OUTPUT_FILE="${OUTPUT_DIR}/Pokec_combined_simulation_output.txt" 
# # 清空所有合并输出文件，确保每次运行都是新的结果
# # 如果您希望保留历史记录，可以注释掉这些行，但请注意文件会持续增长
# > "$CORA_COMBINED_OUTPUT_FILE" 
# > "$PUBMED_COMBINED_OUTPUT_FILE"
# > "$FLICKR_COMBINED_OUTPUT_FILE"
# > "$REDDIT_COMBINED_OUTPUT_FILE" 
# > "$PRO_COMBINED_OUTPUT_FILE"   
# > "$YELP_COMBINED_OUTPUT_FILE"   
# --- 循环运行仿真 ---
for dataset_key in "${SIMULATION_KEYS[@]}"; do
    echo "--- 运行数据集: $dataset_key ---"

    # 解析数据集参数
    IFS=' ' read -r -a params <<< "${DATASET_CONFIGS[$dataset_key]}"

    # 根据 dataset_key 的前缀判断输出文件
    OUTPUT_FILE=""
    if [[ "$dataset_key" == cora_* ]]; then
        OUTPUT_FILE="$CORA_COMBINED_OUTPUT_FILE"
    elif [[ "$dataset_key" == pubmed_* ]]; then
        OUTPUT_FILE="$PUBMED_COMBINED_OUTPUT_FILE"
    elif [[ "$dataset_key" == flickr_* ]]; then
        OUTPUT_FILE="$FLICKR_COMBINED_OUTPUT_FILE"
    elif [[ "$dataset_key" == Reddit_* ]]; then 
        OUTPUT_FILE="$REDDIT_COMBINED_OUTPUT_FILE"
    elif [[ "$dataset_key" == Yelp_* ]]; then   
         OUTPUT_FILE="$YELP_COMBINED_OUTPUT_FILE"
     elif [[ "$dataset_key" == proteins* ]]; then 
        OUTPUT_FILE="$PRO_COMBINED_OUTPUT_FILE"
          elif [[ "$dataset_key" == Pokec* ]]; then 
        OUTPUT_FILE="$POKEC_COMBINED_OUTPUT_FILE"
        # 对于其他新增数据集，为每个配置生成独立的输出文件4
        else
        OUTPUT_FILE="${OUTPUT_DIR}/${dataset_key}_simulation_output.txt" 
    fi

    # 定义当前模拟运行的特定日志文件
    # 这里我们使用一个通用的 run.log，并在每次运行前清空它，或者为每次运行创建唯一的日志文件
    # 选项 1: 每次运行清空并写入同一个 run.log (适用于你只关心最后一次运行日志的情况)
    # CURRENT_RUN_LOG="${LOG_DIR}/run.log"
    # > "$CURRENT_RUN_LOG" # 每次运行前清空日志文件

    # 选项 2: 为每次运行创建唯一的日志文件 (推荐，可以保留所有历史日志)
    CURRENT_RUN_LOG="${LOG_DIR}/${dataset_key}_run.log"


    # 打印即将执行的命令，方便调试
    echo "执行命令: \"$EXECUTABLE\" \"${params[@]}\" \"$OUTPUT_FILE\" | tee \"$CURRENT_RUN_LOG\""
    echo "控制台输出将同时显示在终端和保存到: $CURRENT_RUN_LOG"

    # 执行 C++ 程序，并将所有参数传递过去
    # 使用 `tee` 命令同时将标准输出显示在终端并写入日志文件
    # `2>&1` 将标准错误重定向到标准输出，确保错误信息也被 tee 捕获
    "$EXECUTABLE" "${params[@]}" "$OUTPUT_FILE" | tee "$CURRENT_RUN_LOG" 2>&1

    # 检查上一个命令的退出状态（即 C++ 程序的执行结果）
    if [ $? -eq 0 ]; then
        echo "--- 数据集 $dataset_key 仿真完成。输出已保存到: $OUTPUT_FILE ---"
        echo "--- 详细运行日志已保存到: $CURRENT_RUN_LOG ---"
    else
        echo "--- 数据集 $dataset_key 仿真失败！请检查日志文件: $CURRENT_RUN_LOG 以获取错误信息。 ---"
    fi
    echo ""
done

echo "--- 所有仿真已完成。---"
