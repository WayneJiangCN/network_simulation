import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

# 假设数据文件名为 'data.txt'，且位于脚本相同目录下
file_path = '../mac_u.txt'
# 定义保存文件的路径和名称
save_path = 'utilization_vs_time.png'

# --- 1. 解决 Matplotlib 中文显示问题 ---
# 依次尝试使用支持中文的字体，以解决 'Glyph missing' 警告
plt.rcParams['font.sans-serif'] = ['SimHei', 'WenQuanYi Zen Hei', 'Microsoft YaHei', 'DejaVu Sans']
plt.rcParams['axes.unicode_minus'] = False # 解决负号显示问题

try:
    # 2. 数据读取和清洗
    # 使用 sep=',' 明确告诉 pandas 数据是逗号分隔的
    df = pd.read_csv(file_path, sep=',', header=None, names=['Time', 'Count'])
    print("数据读取成功。\n")

    # 3. 数据清洗和类型转换
    df['Time'] = pd.to_numeric(df['Time'], errors='coerce')
    df['Count'] = pd.to_numeric(df['Count'], errors='coerce')
    df.dropna(inplace=True)
    
    if df.empty:
        print("警告：数据文件读取成功，但 DataFrame 为空，请检查文件内容和格式。")
        exit()

    # 4. 计算利用率
    df['Utilization'] = np.where(df['Time'] != 0, df['Count'] / df['Time']/16/8*100, 0)
    
    print("利用率计算结果（前5行）：")
    print(df.head())
    print("-" * 30)

    # 5. 数据可视化
    plt.figure(figsize=(10, 6))
    
    plt.plot(
        df['Time'],
        df['Utilization'],
        marker='o',
        linestyle='-',
        color='#1f77b4',
        label='Utilization'
    )
    
    # 设置图表标题和标签 (支持中文)
    plt.title('Utilization vs. Time (利用率与时间关系)', fontsize=16)
    plt.xlabel('Time (时间)', fontsize=14)
    plt.ylabel('Utilization (次数/时间)', fontsize=14)
    
    # 添加网格线
    plt.grid(True, linestyle='--', alpha=0.6)
    plt.legend()
    plt.tight_layout()
    
    # 6. 保存图表
    plt.savefig(save_path)
    print(f"\n图表已成功保存到文件: {save_path}")
    
    # 显示图表（可选，如果你的环境支持图形界面）
    # plt.show() 

except FileNotFoundError:
    print(f"错误：文件 '{file_path}' 未找到。请确保文件存在并命名为 '{file_path}'。")
except Exception as e:
    print(f"处理数据或绘图时发生错误: {e}")