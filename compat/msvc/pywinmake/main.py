from utils.logging import get_logger
from utils.script_helpers import getShExecutor, ScriptType

log = get_logger(verbose=True, do_indent=True)
sh = getShExecutor()


def main():
    ret = sh.exec_script(
        script_type=ScriptType.cmd, script="echo", args=["hello world"]
    )
    log.debug(f"ret: {ret}")


if __name__ == "__main__":
    main()
