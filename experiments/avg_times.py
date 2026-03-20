import os
import yaml
import csv
from commons import *
from collections import defaultdict

with open(os.path.join(PROJECT_ROOT,'experiments/experiment.yaml')) as f:
    data = yaml.load(f, Loader=yaml.FullLoader)

dataset_name_list=data['plotting']['dataset']
results_path=f'experiments/results-{version_tag}'
output_folder = os.path.join(PROJECT_ROOT, results_path, 'averaged_results_rcc')
os.makedirs(output_folder, exist_ok=True)

for dataset_name in dataset_name_list:
    folderpath = os.path.join(PROJECT_ROOT, results_path, 'src1', dataset_name)

    grouped = defaultdict(list)

    for filename in os.listdir(folderpath):
        if not filename.endswith(".csv"):
            continue

        filepath = os.path.join(folderpath, filename)
        data_dict = {}

        
        with open(filepath, newline='') as f:
            reader = csv.reader(f)
            for row in reader:
                if len(row) == 2:
                    key, value = row
                    data_dict[key.strip()] = value.strip()

        if "bitlength" not in data_dict or "hamming_weight" not in data_dict or "time_server_crypto" not in data_dict:
            continue

        bitlength = int(data_dict["bitlength"])
        hamming_weight = int(data_dict["hamming_weight"])
        time_server_crypto = float(data_dict["time_server_crypto"])

        grouped[(bitlength, hamming_weight)].append(time_server_crypto)

    result_lines = []
    result_lines.append(f"Results for dataset: {dataset_name}\n")
    result_lines.append(f"{'bitlength':>10} {'hamming_weight':>15} {'avg_time_server_crypto':>25} {'num_samples':>15}\n")

    for (bitlength, hamming_weight), values in sorted(grouped.items()):
        avg_crypto = sum(values) / len(values)
        result_lines.append(f"{bitlength:>10d} {hamming_weight:>15d} {avg_crypto:>25.3f} {len(values):>15d}\n")

    # Write results to text file
    output_file = os.path.join(output_folder, f"{dataset_name}_averaged.txt")
    with open(output_file, "w") as f:
        f.writelines(result_lines)


output_folder = os.path.join(PROJECT_ROOT, results_path, 'averaged_results_xxcmp')
os.makedirs(output_folder, exist_ok=True)

for dataset_name in dataset_name_list:
    folderpath = os.path.join(PROJECT_ROOT, results_path, 'src2', dataset_name)

    grouped = defaultdict(list)

    for filename in os.listdir(folderpath):
        if not filename.endswith(".csv"):
            continue

        filepath = os.path.join(folderpath, filename)
        data_dict = {}

        with open(filepath, newline='') as f:
            reader = csv.reader(f)
            for row in reader:
                if len(row) == 2:
                    key, value = row
                    data_dict[key.strip()] = value.strip()

        if "bitlength" not in data_dict or "time_server" not in data_dict:
            continue

        bitlength = int(data_dict["bitlength"])
        time_server = float(data_dict["time_server"])

        grouped[bitlength].append(time_server)

    # Prepare readable results
    result_lines = []
    result_lines.append(f"Results for dataset: {dataset_name}\n")
    result_lines.append(f"{'bitlength':>10} {'avg_time_server':>20} {'num_samples':>15}\n")

    for bitlength in sorted(grouped.keys()):
        avg_server_time = sum(grouped[bitlength]) / len(grouped[bitlength])
        result_lines.append(f"{bitlength:>10d} {avg_server_time:>20.3f} {len(grouped[bitlength]):>15d}\n")

    output_file = os.path.join(output_folder, f"{dataset_name}_averaged.txt")
    with open(output_file, "w") as f:
        f.writelines(result_lines)

