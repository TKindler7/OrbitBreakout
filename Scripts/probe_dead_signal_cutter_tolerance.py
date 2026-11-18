"""Controlled PIE comparison: observed natural stall, then native AI MoveToLocation
with the same goal and acceptance288 versus45. Machine perception tick pauses
during comparison; AI/collision/character movement remain active. Diagnostic
transient setup only; campaign hash protected. Root must run in a fresh PIE.
"""
import unreal as u
import json, math, time, traceback, hashlib
from pathlib import Path


class CutterToleranceProbe:
    def __init__(self):
        self.editor=u.get_editor_subsystem(u.UnrealEditorSubsystem)
        self.levels=u.get_editor_subsystem(u.LevelEditorSubsystem)
        self.saved=Path(u.Paths.project_saved_dir())
        self.out=self.saved/'Level2'/'CUTTER_TOLERANCE_PROBE.json'
        self.campaign=self.saved/'SaveGames'/'OB_EmergencyDeparture.sav'
        self.report={'status':'STARTING','scope':'Controlled tolerance comparison after observing the natural north Cutter stall; actor perception tick disabled only for comparison, AI movement/collision unchanged',
                     'campaign_hash_before':self.hash_campaign(),'samples':[],'nearby_collision':[],'errors':[]}
        self.handle=None;self.started=time.monotonic();self.phase='waiting';self.player=None
        self.next_sample=0;self.enemy=None;self.ai=None

    def hash_campaign(self):
        return hashlib.sha256(self.campaign.read_bytes()).hexdigest() if self.campaign.exists() else 'ABSENT'

    @staticmethod
    def xyz(v):return [round(float(v.x),2),round(float(v.y),2),round(float(v.z),2)]

    @staticmethod
    def prop(o,n):return o.get_editor_property(n)

    def write(self):
        self.out.parent.mkdir(parents=True,exist_ok=True)
        self.out.write_text(json.dumps(self.report,indent=2)+'\n')

    def trace_summary(self,result):
        values=result if isinstance(result,tuple) else (result,)
        hit=next((v for v in values if isinstance(v,u.HitResult)),None)
        if hit is None:return {'blocking':False,'binding_return_type':type(result).__name__}
        fields=hit.to_dict()
        actor=fields.get('hit_actor');component=fields.get('hit_component')
        def vector_field(name):
            value=fields.get(name)
            return self.xyz(value) if value is not None else None
        def object_name(value):
            return value.get_name() if value is not None and hasattr(value,'get_name') else str(value) if value is not None else None
        return {'blocking':bool(fields.get('blocking_hit',False)),
                'initial_overlap':bool(fields.get('initial_overlap',False)),
                'distance':float(fields.get('distance',0.0)),
                'location':vector_field('location'),'impact':vector_field('impact_point'),
                'normal':vector_field('impact_normal'),
                'actor':object_name(actor),
                'actor_label':actor.get_actor_label() if actor is not None and hasattr(actor,'get_actor_label') else None,
                'component':object_name(component)}

    def get_nearby_collision(self,world):
        center=u.Vector(19600,9700,70)
        for actor in u.GameplayStatics.get_all_actors_of_class(world,u.Actor):
            origin,extent=actor.get_actor_bounds(True)
            if max(extent.x,extent.y,extent.z)<=0:continue
            dx=max(abs(origin.x-center.x)-extent.x,0)
            dy=max(abs(origin.y-center.y)-extent.y,0)
            dz=max(abs(origin.z-center.z)-extent.z,0)
            if math.sqrt(dx*dx+dy*dy+dz*dz)>650:continue
            components=[]
            for component in actor.get_components_by_class(u.PrimitiveComponent):
                enabled=component.get_collision_enabled()
                if enabled==u.CollisionEnabled.NO_COLLISION:continue
                components.append({'name':component.get_name(),'collision':str(enabled),
                    'visibility_response':str(component.get_collision_response_to_channel(u.CollisionChannel.ECC_VISIBILITY)),
                    'pawn_response':str(component.get_collision_response_to_channel(u.CollisionChannel.ECC_PAWN))})
            self.report['nearby_collision'].append({'name':actor.get_name(),'label':actor.get_actor_label(),
                'origin':self.xyz(origin),'extent':self.xyz(extent),'components':components})

    def sample(self,world):
        position=self.enemy.get_actor_location();player_position=self.player.get_actor_location()
        capsule=self.enemy.get_component_by_class(u.CapsuleComponent)
        movement=self.enemy.get_component_by_class(u.CharacterMovementComponent)
        path=u.AIHelperLibrary.get_current_path(self.ai)
        points=u.AIHelperLibrary.get_current_path_points(self.ai)
        sight=u.SystemLibrary.line_trace_single(world,position+u.Vector(0,0,8),player_position+u.Vector(0,0,24),
            u.TraceTypeQuery.TRACE_TYPE_QUERY1,False,[self.enemy],u.DrawDebugTrace.NONE,True)
        sweep=u.SystemLibrary.capsule_trace_single(world,position,u.Vector(player_position.x,player_position.y,position.z),
            capsule.get_scaled_capsule_radius(),capsule.get_scaled_capsule_half_height(),
            u.TraceTypeQuery.TRACE_TYPE_QUERY1,False,[self.enemy,self.player],u.DrawDebugTrace.NONE,True)
        sight_summary=self.trace_summary(sight)
        native_ray_clear=not sight_summary['blocking'] or sight_summary.get('actor')==self.player.get_name()
        self.report['samples'].append({'elapsed':round(time.monotonic()-self.activated,3),'phase':self.phase,
            'state':str(self.prop(self.enemy,'state')),'position':self.xyz(position),'player':self.xyz(player_position),
            'distance_to_player_2d':round(math.hypot(position.x-player_position.x,position.y-player_position.y),2),
            'velocity':self.xyz(self.enemy.get_velocity()),'movement_mode':str(self.prop(movement,'movement_mode')),
            'max_walk_speed':float(self.prop(movement,'max_walk_speed')),
            'ai_move_status':str(self.ai.get_move_status()),'ai_immediate_destination':self.xyz(self.ai.get_immediate_move_destination()),
            'path_valid':bool(path and path.is_valid()),'path_partial':bool(path and path.is_partial()),
            'path_index':int(u.AIHelperLibrary.get_current_path_index(self.ai)),
            'path_points':[self.xyz(p) for p in points],
            'controller_line_of_sight':bool(self.ai.line_of_sight_to(self.player,position+u.Vector(0,0,8),False)),
            'machine_equivalent_visibility_ray_clear':native_ray_clear,'sight_trace':sight_summary,
            'direct_capsule_sweep':self.trace_summary(sweep)})
        self.write()

    def tick(self,dt):
        try:
            if time.monotonic()-self.started>90:raise TimeoutError('Probe exceeded90seconds')
            world=self.editor.get_game_world()
            if not world:return
            player=u.GameplayStatics.get_player_character(world,0)
            directors=u.GameplayStatics.get_all_actors_of_class(world,u.OBLevel2Director)
            if not player or not directors:return
            self.player=player;director=directors[0]
            if self.phase=='waiting':
                gi=u.GameplayStatics.get_game_instance(world)
                if not self.prop(gi,'development_chapter_session'):return
                if director.is_cinematic_active():director.finish_cinematic()
                if not player.is_gameplay_active():return
                player.set_editor_property('max_health',10000.0);player.add_health(10000)
                player.set_actor_location(u.Vector(19300,9500,110),False,True)
                self.ready_at=time.monotonic()+6.5
                self.phase='grace';self.report['status']='WAITING_FOR_GRACE';self.write()
            elif self.phase=='grace' and time.monotonic()>=self.ready_at:
                machines=u.GameplayStatics.get_all_actors_of_class(world,u.OBMachineEnemy)
                self.enemy=next(m for m in machines if str(self.prop(m,'stable_id'))=='L2_E211_04')
                self.get_nearby_collision(world)
                self.enemy.activate();self.ai=self.enemy.get_controller()
                if not self.ai:raise RuntimeError('Cutter has no AI controller')
                self.activated=time.monotonic();self.next_sample=self.activated;self.phase='observe'
                self.report['status']='OBSERVING';self.report['initial_position']=self.xyz(self.enemy.get_actor_location())
                u.log('OB_CUTTER_TOLERANCE_PROBE_OBSERVING')
            elif self.phase=='observe':
                now=time.monotonic()
                if now>=self.next_sample:
                    self.next_sample=now+.2;self.sample(world)
                if now-self.activated>=11:
                    self.enemy.set_actor_tick_enabled(False)
                    self.comparison_goal=self.player.get_actor_location()
                    self.report['comparison_start_position']=self.xyz(self.enemy.get_actor_location())
                    result=self.ai.move_to_location(self.comparison_goal,288.0,True,True,True,True,None,False)
                    self.report['request_288_result']=str(result)
                    self.phase='tolerance_288';self.phase_started=now;self.sample(world)
            elif self.phase=='tolerance_288':
                now=time.monotonic()
                if now>=self.next_sample:
                    self.next_sample=now+.2;self.sample(world)
                if now-self.phase_started>=1.0:
                    self.report['after_288_position']=self.xyz(self.enemy.get_actor_location())
                    result=self.ai.move_to_location(self.comparison_goal,45.0,True,True,True,True,None,False)
                    self.report['request_45_result']=str(result)
                    self.phase='tolerance_45';self.phase_started=now;self.sample(world)
            elif self.phase=='tolerance_45':
                now=time.monotonic()
                if now>=self.next_sample:
                    self.next_sample=now+.2;self.sample(world)
                if now-self.phase_started>=6.0:self.finish(True)
        except Exception:self.finish(False,traceback.format_exc())

    def finish(self,success,error=None):
        if self.handle is not None:u.unregister_slate_post_tick_callback(self.handle);self.handle=None
        if self.enemy and u.SystemLibrary.is_valid(self.enemy):self.enemy.set_actor_tick_enabled(True)
        if self.player and u.SystemLibrary.is_valid(self.player):self.player.set_editor_property('max_health',100.0)
        self.report['campaign_hash_after']=self.hash_campaign()
        if self.report['campaign_hash_after']!=self.report['campaign_hash_before']:
            success=False;self.report['errors'].append('Campaign hash changed unexpectedly')
        if error:self.report['errors'].append(error)
        self.report['status']='OBSERVED' if success else 'FAIL';self.write()
        u.log('OB_CUTTER_TOLERANCE_PROBE_FINISHED '+self.report['status']+' '+str(self.out))
        if self.editor.get_game_world():self.levels.editor_request_end_play()

    def start(self):
        if self.editor.get_game_world():raise RuntimeError('Stop prior PIE before the focused Cutter probe')
        if 'L_DeadSignal' not in self.editor.get_editor_world().get_name():raise RuntimeError('Load L_DeadSignal first')
        self.write();self.handle=u.register_slate_post_tick_callback(self.tick);self.levels.editor_request_begin_play()


import builtins
prior=getattr(builtins,'_orbit_cutter_tolerance_probe',None)
if prior and getattr(prior,'handle',None) is not None:raise RuntimeError('Cutter probe already active')
_probe=CutterToleranceProbe();builtins._orbit_cutter_tolerance_probe=_probe;_probe.start()
