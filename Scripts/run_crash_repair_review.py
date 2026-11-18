from pathlib import Path
import unreal as u
root=Path(u.Paths.project_dir()).resolve()
for name in ['probe_crash_recovery_assets.py','patch_dead_signal_crash_motion.py','preview_dead_signal_crash_motion.py']:
 p=root/'Scripts'/name
 exec(compile(p.read_text(),str(p),'exec'),{'__name__':'__main__'})
