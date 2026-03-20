import os

import matplotlib.pyplot as plt
import pandas as pd
import yaml
from commons import *

num_attributes={
    "breast": 30,
    "steel": 33,
    "heart": 13,
    "spam": 57,
}

def get_points_list(x, y):
    assert len(x.values) == len(y.values)
    return [(x.values[i], y.values[i]) for i in range(len(x.values))]

def read_directory(dir_path):
    res=[]
    for path in os.listdir(dir_path):
        # check if current path is a file
        if os.path.isfile(os.path.join(dir_path, path)):
            x = pd.read_csv(os.path.join(dir_path, path), names=['metric', 'value'], index_col=0).T
            res.append(x)
    if len(res) > 0:
        return pd.concat(res)
    else:
        return None    
        
def read_sortinghats(dir_path):
    res=[]
    for path in os.listdir(dir_path):
        # check if current path is a file
        if os.path.isfile(os.path.join(dir_path, path)):
            x = pd.read_csv(os.path.join(dir_path, path))
            res.append(x)
    if len(res) > 0:
        return pd.concat(res)
    else:
        return None   

with open(os.path.join(PROJECT_ROOT,'experiments/experiment.yaml')) as f:
    data = yaml.load(f, Loader=yaml.FullLoader)

    dataset_name_list=data['plotting']['dataset']
    results_path=f'experiments/results-{version_tag}'

    # Create a dictionary with a list of upper and lower bounds for each dataset
    time_bounds = {
        'breast': None,
        'steel': None,
        'heart': None,
        'spam': None,
    }
    
    comm_bounds = {
        'breast': None,
        'steel': None,
        'heart': None,
        'spam': None,       
    }
    
    f1, plt_agg_RCC=plt.subplots()
    f2, plt_agg_XXCMP=plt.subplots()
    for dataset_name in dataset_name_list:
        print(dataset_name)

        f3, plt_time_RCC=plt.subplots()
        f4, plt_comm=plt.subplots()
        f5, plt_time_XXCMP=plt.subplots()
        
        
        
        # Get the current figure size in inches
        current_fig_size = plt.gcf().get_size_inches()

        # Set the desired aspect ratio: height = 0.5 * width
        desired_width = current_fig_size[0]
        desired_height = desired_width / 1.618

        # Update the figure size with the new dimensions
        f1.set_size_inches(desired_width, desired_height)
        f2.set_size_inches(desired_width, desired_height)
        f3.set_size_inches(desired_width, desired_height)   
        f4.set_size_inches(desired_width, desired_height)
        f5.set_size_inches(desired_width, desired_height)

             
        # src1

        raw_data=read_directory(os.path.join(PROJECT_ROOT, results_path, 'src1', dataset_name))
        
        raw_data=raw_data[(2 <= raw_data["bitlength"]) & (raw_data["bitlength"]<=16)]
        
        if raw_data is not None:
            # Keep correct results, average them
            raw_data=raw_data[raw_data["correctness"]==1]
            raw_data['time_noagg'] = raw_data['time_server_crypto'] - raw_data['time_server_aggregation']
            data_avg=raw_data.groupby(['bitlength', 'hamming_weight', 'comparison'], as_index=False).mean()
            data_std=raw_data.groupby(['bitlength', 'hamming_weight', 'comparison'], as_index=False).std()
            data_avg2=raw_data.groupby(['bitlength', 'comparison'], as_index=False).mean()
            data_std2=raw_data.groupby(['bitlength', 'comparison'], as_index=False).std()
            
            # Range cover
            for hamming_weight in [2,4]:
                filtered_avg=data_avg[(data_avg["hamming_weight"]==hamming_weight) & (data_avg["comparison"]==0)]
                filtered_std=data_std[(data_std["hamming_weight"]==hamming_weight) & (data_std["comparison"]==0)]
                
                plt_time_RCC.fill_between(
                    filtered_avg['bitlength'],
                    (filtered_avg['time_noagg'] - filtered_std['time_noagg'])/1000, 
                    (filtered_avg['time_noagg'] + filtered_std['time_noagg'])/1000,
                    alpha=0.2
                )

                plt_time_RCC.plot(filtered_avg['bitlength'], filtered_avg['time_noagg']/1000, label=f'RCC-PRFE (h={hamming_weight})')
                plt_comm.plot(filtered_avg['bitlength'], filtered_avg['comm_query']/1000, label=f'RCC-PRFE (h={hamming_weight})')
                
            plt_agg_RCC.fill_between(
                data_avg2['bitlength'],
                (data_avg2['time_server_aggregation'] - data_std2['time_server_aggregation'])/1000, 
                (data_avg2['time_server_aggregation'] + data_std2['time_server_aggregation'])/1000,
                alpha=0.2
            )
            plt_agg_RCC.plot(data_avg2['bitlength'], data_avg2['time_server_aggregation']/1000, label=f'RCC-PRFE ({dataset_name} ({num_attributes[dataset_name]} attributes))')
                    

        #src2

        raw_data_xcmp=read_directory(os.path.join(PROJECT_ROOT, results_path, 'src2', dataset_name))
        raw_data_xcmp=raw_data_xcmp[(2 <= raw_data_xcmp["bitlength"] )&( raw_data_xcmp["bitlength"]<=16)]
        
        
        if raw_data_xcmp is not None:

            # Only valid entries
            raw_data_xcmp = raw_data_xcmp[raw_data_xcmp['correctness']==1]
            raw_data_xcmp['time_noagg'] = raw_data_xcmp['time_server'] - raw_data_xcmp['time_server_aggregation']
            data_xcmp_avg=raw_data_xcmp.groupby(['bitlength', 'mult_path'], as_index=False).mean()
            data_xcmp_std=raw_data_xcmp.groupby(['bitlength', 'mult_path'], as_index=False).std()

            # Sum Path
            filtered=data_xcmp_avg[data_xcmp_avg['mult_path']==0]
            filtered_std=data_xcmp_std[data_xcmp_std['mult_path']==0]
            plt_time_XXCMP.fill_between(
                filtered['bitlength'],
                (filtered['time_noagg'] - filtered_std['time_noagg'])/1000,
                (filtered['time_noagg'] + filtered_std['time_noagg'])/1000,
                alpha=0.2
            )

            plt_agg_XXCMP.fill_between(
                filtered['bitlength'],
                (filtered['time_server_aggregation'] - filtered_std['time_server_aggregation'])/1000,
                (filtered['time_server_aggregation'] + filtered_std['time_server_aggregation'])/1000,
                alpha=0.2
            )

            plt_time_XXCMP.plot(filtered['bitlength'], filtered['time_noagg']/1000, label=f'XXCMP-PRFE')
            plt_comm.plot(filtered['bitlength'], filtered['comm_request']/1000, label=f'XXCMP-PRFE')
            plt_agg_XXCMP.plot(filtered['bitlength'], filtered['time_server_aggregation']/1000, label=f'XXCMP-PRFE ({dataset_name} ({num_attributes[dataset_name]} attributes))')
        
        figures_dir=os.path.join(PROJECT_ROOT, results_path, 'figures')
        if not os.path.exists(figures_dir):
            os.makedirs(figures_dir)

        plt_time_RCC.set_ylabel("Seconds")
        plt_time_RCC.set_xlabel("Bit Precision")
        plt_time_RCC.set_xticks(filtered_avg['bitlength'])
        plt_time_RCC.legend()
        if time_bounds[dataset_name] is not None:
            # set the yaxis range
            plt_time_RCC.set_ylim(time_bounds[dataset_name][0], time_bounds[dataset_name][1])
        plt_time_RCC.set_title(f"Inference Time for {dataset_name.capitalize()} dataset ({num_attributes[dataset_name]} attributes)")
        f3.savefig(os.path.join(figures_dir, f"time-{dataset_name}.svg"), bbox_inches = 'tight')
        plt.close()

        
        plt_time_XXCMP.set_ylabel("Seconds")
        plt_time_XXCMP.set_xlabel("Bit Precision")
        plt_time_XXCMP.set_xticks(filtered['bitlength'])
        plt_time_XXCMP.legend()
        if time_bounds[dataset_name] is not None:
            # set the yaxis range
            plt_time_XXCMP.set_ylim(time_bounds[dataset_name][0], time_bounds[dataset_name][1])
        plt_time_XXCMP.set_title(f"Inference Time for {dataset_name.capitalize()} dataset ({num_attributes[dataset_name]} attributes)")
        f5.savefig(os.path.join(figures_dir, f"time-x-{dataset_name}.svg"), bbox_inches = 'tight')
        plt.close()
        
        
        plt_comm.set_ylabel("KBytes")
        plt_comm.set_xlabel("Bit Precision")
        plt_comm.set_xticks(filtered_avg['bitlength'])
        plt_comm.legend()
        plt_comm.set_yscale('log')
        if comm_bounds[dataset_name] is not None:
            # set the yaxis range
            plt_comm.set_ylim(comm_bounds[dataset_name][0], comm_bounds[dataset_name][1])
        plt_comm.set_title(f"Query communication cost for {dataset_name.capitalize()} dataset ({num_attributes[dataset_name]} attributes)")
        f4.savefig(os.path.join(figures_dir, f"comm-query-{dataset_name}.svg"), bbox_inches = 'tight')
        plt.close()        

    plt_agg_RCC.set_ylabel("Seconds")
    plt_agg_RCC.set_xlabel("Bit Precision")
    plt_agg_RCC.set_xticks(filtered_avg['bitlength'])
    plt_agg_RCC.legend()
    if time_bounds[dataset_name] is not None:
        # set the yaxis range
        plt_agg_RCC.set_ylim(time_bounds[dataset_name][0], time_bounds[dataset_name][1])
    plt_agg_RCC.set_title(f"Aggregation Time for all datasets")
    f1.savefig(os.path.join(figures_dir, f"agg_time-{dataset_name}.svg"), bbox_inches = 'tight')
    plt.close()

    plt_agg_XXCMP.set_ylabel("Seconds")
    plt_agg_XXCMP.set_xlabel("Bit Precision")
    plt_agg_XXCMP.set_xticks(filtered['bitlength'])
    plt_agg_XXCMP.legend()
    if time_bounds[dataset_name] is not None:
        # set the yaxis range
        plt_agg_XXCMP.set_ylim(time_bounds[dataset_name][0], time_bounds[dataset_name][1])
    plt_agg_XXCMP.set_title(f"Aggregation Time for all datasets")
    f2.savefig(os.path.join(figures_dir, f"agg_time-x-{dataset_name}.svg"), bbox_inches = 'tight')
    plt.close()
        

