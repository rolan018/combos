from python.analyzers.timeline_analyzer import union_all_statuses


OUTPUT_FILE_NAME = "merged_timeline.csv"

def merge_timeline(input_path,
                   flops, 
                   props,
                   output_path=None,
                   output_file_name=OUTPUT_FILE_NAME):
    if not output_path:
        output_path = f"{input_path}/{output_file_name}"
    with open(output_path, "w") as fout:
        print(f"flop,prop,client_name,status,start,end,duration", file=fout)
        for flop in flops:
            for prop in props:
                dir_input = f"{input_path}/execute_{flop}_{prop}.csv"
                try:
                    for cl, tp, start, end, duration in union_all_statuses(dir_input):
                        print(f"{flop},{prop},{cl},{tp},{start},{end},{duration}", file=fout)
                except FileNotFoundError:
                    print("Not found:", f"execute_{flop}_{prop}.csv")