"""Read-only, one-tick navigation binding probe in an existing Dead Signal PIE.

Run after the full smoke has stopped. Does not move actors, change gameplay,
start/stop PIE or save assets. Uses unchanged maintenance floor points, so a
separate far-away visual lineup does not affect the query.
"""
import builtins
import json
import traceback
from datetime import datetime, timezone
from pathlib import Path
import unreal as u


class NavPythonContextProbe:
    def __init__(self):
        self.handle = None
        self.out = Path(u.Paths.project_saved_dir())/'Level2'/'NAV_PYTHON_CONTEXT_PROBE.json'

    @staticmethod
    def xyz(v):
        return [round(float(v.x), 3), round(float(v.y), 3), round(float(v.z), 3)]

    @staticmethod
    def projected(result):
        if hasattr(result, 'x'):
            return result
        if isinstance(result, tuple) and not any(v is False for v in result):
            return next((v for v in result if hasattr(v, 'x')), None)
        return None

    def tick(self, dt):
        u.unregister_slate_post_tick_callback(self.handle)
        self.handle = None
        report = {'status': 'STARTING', 'utc': datetime.now(timezone.utc).isoformat(),
            'scope': 'Read-only navigation calls on the actual PIE-owned NavigationSystemV1',
            'actor_mutations': False, 'errors': []}
        u.log('OB_NAV_PYTHON_CONTEXT_PROBE_BEGIN')
        try:
            world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_game_world()
            if not world or 'DeadSignal' not in world.get_name():
                raise RuntimeError('Requires an existing Dead Signal PIE world')
            # ObjectIterator/get_outer are native Python object utilities. They
            # do not execute a NavigationSystem function on its class default.
            systems = [obj for obj in u.ObjectIterator(u.NavigationSystemV1) if obj.get_outer() == world]
            if len(systems) != 1:
                raise RuntimeError('Expected one PIE-owned navigation system, got '+str(len(systems)))
            nav = systems[0]
            report['world'] = world.get_path_name()
            report['navigation_object'] = nav.get_path_name()
            report['navigation_outer'] = nav.get_outer().get_path_name()
            points = [u.Vector(1800, 0, 10), u.Vector(2200, 0, 10)]
            projected = []
            report['projections'] = []
            for point in points:
                # call_method binds ProcessEvent to nav, unlike the generated
                # static Python class method, which binds it to the CDO.
                result = nav.call_method('K2_ProjectPointToNavigation',
                    args=(world, point, None, None, u.Vector(90, 90, 80)))
                location = self.projected(result)
                report['projections'].append({'input': self.xyz(point),
                    'return_type': type(result).__name__,
                    'projected': self.xyz(location) if location is not None else None})
                if location is None:
                    raise RuntimeError('Maintenance floor projection failed')
                projected.append(location)
            path = nav.call_method('FindPathToLocationSynchronously',
                args=(world, projected[0], projected[1], None, None))
            report['path_valid'] = bool(path and path.is_valid())
            report['path_partial'] = bool(path and path.is_partial())
            report['path_points'] = [self.xyz(p) for p in path.get_editor_property('path_points')] if path else []
            if not report['path_valid'] or report['path_partial']:
                raise RuntimeError('Maintenance route is not complete')
            report['status'] = 'PASS'
        except Exception:
            report['status'] = 'FAIL'
            report['errors'].append(traceback.format_exc())
        self.out.parent.mkdir(parents=True, exist_ok=True)
        self.out.write_text(json.dumps(report, indent=2)+'\n')
        u.log('OB_NAV_PYTHON_CONTEXT_PROBE_END '+report['status']+' '+str(self.out))

    def start(self):
        smoke = getattr(builtins, '_orbit_dead_signal_smoke', None)
        if smoke and getattr(smoke, 'handle', None) is not None:
            raise RuntimeError('Wait until the full mission smoke has finished')
        self.handle = u.register_slate_post_tick_callback(self.tick)


previous = getattr(builtins, '_orbit_nav_python_context_probe', None)
if previous and getattr(previous, 'handle', None) is not None:
    raise RuntimeError('Navigation context probe is already queued')
probe = NavPythonContextProbe()
builtins._orbit_nav_python_context_probe = probe
probe.start()
