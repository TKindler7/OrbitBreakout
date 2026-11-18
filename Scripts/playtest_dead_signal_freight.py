"""Focused saved-map freight reinforcement regression; run after the full mission smoke."""
import ast
from pathlib import Path
import unreal as u
import time
source=Path(u.Paths.project_dir())/'Scripts'/'playtest_dead_signal.py'
tree=ast.parse(source.read_text(),str(source))
tree.body=[n for n in tree.body if isinstance(n,(ast.Import,ast.ImportFrom,ast.ClassDef))]
exec(compile(tree,str(source),'exec'),globals())
class FreightSmoke(DeadSignalSmoke):
    def __init__(self):
        super().__init__()
        self.out=self.saved/'Level2'/'FREIGHT_RUNTIME_SMOKE.json'
        self.report['scope']='Focused saved-map freight205 hidden arrival and attack regression; prerequisites set only in transient PIE'
        self.report['methods']=['Native queue triggered by deaths in encounter204','Transient junction prerequisites and health for focused observation','Native point damage clears fight; not a normal weapon playthrough','Campaign hash protected']
        self.queued_at=None
        self.activated_at=None
    def monitor(self):
        elapsed=time.monotonic()-self.started
        current=[m for m in self.machines() if int(self.prop(m,'encounter_id'))==204]
        if sum(bool(m.is_dead()) for m in current)>=2 and self.queued_at is None:self.queued_at=elapsed
        m=next(m for m in self.machines() if int(self.prop(m,'encounter_id'))==205)
        if self.prop(m,'active') and self.activated_at is None:self.activated_at=elapsed
    def tick(self,dt):
        super().tick(dt)
        if self.handle is not None and self.director:self.monitor()
    def run(self):
        yield from self.wait('Start isolated focused PIE',lambda:self.player and self.director and self.gi and self.director.is_cinematic_active(),70)
        self.check('Isolated developer session',self.prop(self.gi,'development_chapter_session'))
        self.director.finish_cinematic()
        yield from self.delay(2.5,'Native spawn grace')
        self.set_observation_health()
        self.director.set_editor_property('junction_drained',True)
        self.director.set_editor_property('junction_isolated',True)
        self.teleport(u.Vector(9700,3000,110),'Freight center; holds view toward entry')
        self.controller.set_control_rotation(u.Rotator(pitch=0,yaw=0,roll=0))
        self.director.enter_zone(4,False)
        yield from self.fight([204,205],'Observe hidden freight arrival',35)
        self.check('Reinforcement queued and activated',self.queued_at is not None and self.activated_at is not None)
        elapsed=self.activated_at-self.queued_at
        self.check('Hidden reinforcement arrives promptly',elapsed<5,{'seconds_after_trigger':round(elapsed,3)})
        obs=self.states_seen.get(self.current_run+':L2_E205_01',{})
        self.check('Bulwark reached attack telegraph',any('WINDUP' in s for s in obs.get('states',[])))
        self.check('Campaign save unchanged',self.hash_saves()==self.report['campaign_hash_before'])
        self.finish(True)
import builtins
probe=FreightSmoke()
builtins._orbit_dead_signal_freight=probe
probe.start()
