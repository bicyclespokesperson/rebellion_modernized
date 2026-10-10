"""ROOT = this checkout (may be a git worktree); MAIN = the primary checkout owning shared untracked state."""
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MAIN = Path(subprocess.run(["git", "-C", str(ROOT), "rev-parse", "--path-format=absolute", "--git-common-dir"],
                           capture_output=True, text=True, check=True).stdout.strip()).parent
