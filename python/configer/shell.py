import subprocess

def run_in_schell(cmd: str):
    command = cmd
    try:
        result = subprocess.run(command, shell=True, check=False,
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        return result.stdout
    except subprocess.CalledProcessError as e:
        raise e