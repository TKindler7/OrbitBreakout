"""Real-PIE mission smoke for Dead Signal. Run from the editor Python console.

Uses the saved map and native mission, enemies, hazards, holds, cinematics and
reload. Navigation probes and input-driven walks through all opened mission gates
are supplemented by explicit teleports to combat/interaction observation points. Enemy damage is injected through ApplyPointDamage; it is NOT a manual
weapon/controller playthrough. Only the PIE player's health is enlarged. Timed
holds run in real time via UpdateHold, temporarily suspending the pawn actor tick
so the physical-key watchdog does not cancel the synthetic input. World, AI,
physics, damage interruption, range/LOS checks and the controller remain active.

Refuses an already running PIE session or a non-DeadSignal editor map. Direct map
entry must select the existing isolated development save. Never edits a save
file, calls NewMission, or saves assets. Ends PIE after success/failure.
"""
import unreal as u
import hashlib
import json
import math
import time
import traceback
from datetime import datetime, timezone
from pathlib import Path


class DeadSignalSmoke:
    def __init__(self):
        self.saved = Path(u.Paths.project_saved_dir())
        self.out = self.saved / 'Level2' / 'RUNTIME_SMOKE.json'
        self.out.parent.mkdir(parents=True, exist_ok=True)
        self.editor = u.get_editor_subsystem(u.UnrealEditorSubsystem)
        self.levels = u.get_editor_subsystem(u.LevelEditorSubsystem)
        self.started = time.monotonic()
        self.last_write = 0.0
        self.dt = 0.0
        self.handle = None
        self.world = self.player = self.controller = self.director = self.gi = None
        self.navigation_system = None
        self.hold_player = None
        self.active_seen = {}
        self.last_damage = {}
        self.states_seen = {}
        self.trooper_active_seen = set()
        self.current_run = 'initial'
        self.stage = 'initializing'
        self.frames = {}
        self.report = {
            'status': 'STARTING', 'started_utc': datetime.now(timezone.utc).isoformat(),
            'scope': 'Automated PIE mission smoke, not a manual combat/controller playthrough',
            'methods': ['Native saved map and native mission state machine',
                        'Opening and ending cinematics watched without skipping',
                        'Movement-input traversal across all four opened mission gates plus the first route',
                        'Explicitly logged teleports between mission test positions',
                        'Native ApplyPointDamage injects enemy damage; weapon traces not tested',
                        'Real-time native holds with temporary pawn-tick suspension',
                        'Transient PIE player MaxHealth=10000 for observation; actual death/reload tested',
                        'Trooper skeletal assets, native animation class, capsule envelope and live rifle/muzzle checks',
                        'Direct Level2 development save; campaign file SHA-256 protected'],
            'checks': [], 'events': [], 'warnings': [], 'errors': [],
            'teleports': [], 'navigation': [], 'enemy_observations': {},
            'runs': {}, 'performance': {},
            'trooper_roster_audit': [], 'trooper_active_audit': {},
        }
        self.save_paths = [self.saved / 'SaveGames' / 'OB_EmergencyDeparture.sav']
        self.report['campaign_hash_before'] = self.hash_saves()
        pre_trooper = self.out.with_name('PRE_TROOPER_RUNTIME_SMOKE.json')
        if pre_trooper.exists():
            self.report['pre_trooper_evidence_archive'] = {
                'path': str(pre_trooper), 'sha256': hashlib.sha256(pre_trooper.read_bytes()).hexdigest()}

    @staticmethod
    def prop(obj, name):
        return obj.get_editor_property(name)

    @staticmethod
    def xyz(vec):
        return [round(float(vec.x), 2), round(float(vec.y), 2), round(float(vec.z), 2)]

    @staticmethod
    def distance(a, b):
        return math.sqrt((a.x-b.x)**2 + (a.y-b.y)**2 + (a.z-b.z)**2)

    @staticmethod
    def direction(a, b):
        length = max(.001, DeadSignalSmoke.distance(a, b))
        return u.Vector((b.x-a.x)/length, (b.y-a.y)/length, (b.z-a.z)/length)

    def hash_saves(self):
        return {str(p): hashlib.sha256(p.read_bytes()).hexdigest() if p.exists() else 'ABSENT'
                for p in self.save_paths}

    def check(self, description, value, details=None):
        self.report['checks'].append({'check': description, 'pass': bool(value), 'details': details})
        if not value:
            raise RuntimeError(description + (': ' + str(details) if details is not None else ''))

    def event(self, name, **details):
        self.report['events'].append({'elapsed': round(time.monotonic()-self.started, 2),
                                      'run': self.current_run, 'event': name, **details})
        u.log('OB_LEVEL2_SMOKE ' + name + ' ' + json.dumps(details))

    def write(self):
        self.report['stage'] = self.stage
        self.report['elapsed_seconds'] = round(time.monotonic()-self.started, 2)
        self.report['enemy_observations'] = self.states_seen
        self.out.write_text(json.dumps(self.report, indent=2) + '\n')
        self.last_write = time.monotonic()

    def refresh(self):
        world = self.editor.get_game_world()
        if world != self.world:
            self.navigation_system = None
        self.world = world
        if not self.world:
            self.player = self.director = self.controller = self.gi = None
            return
        self.player = u.GameplayStatics.get_player_character(self.world, 0)
        self.controller = u.GameplayStatics.get_player_controller(self.world, 0)
        directors = u.GameplayStatics.get_all_actors_of_class(self.world, u.OBLevel2Director)
        self.director = directors[0] if directors else None
        self.gi = u.GameplayStatics.get_game_instance(self.world)

    def machines(self):
        return list(self.prop(self.director, 'machines')) if self.director else []

    def trooper_components(self, machine):
        mesh = machine.get_component_by_class(u.SkeletalMeshComponent)
        rifles = []
        for component in machine.get_components_by_class(u.StaticMeshComponent):
            asset = self.prop(component, 'static_mesh')
            if asset and asset.get_path_name().startswith('/Game/Weapons/Rifle/Meshes/SM_Rifle.'):
                rifles.append(component)
        return mesh, rifles

    @staticmethod
    def rifle_bone_attachment(mesh, rifle):
        parent = rifle.get_attach_parent()
        mount = rifle if parent == mesh else parent
        return mount, bool(mount and mount.get_attach_parent() == mesh)

    def visible_legacy_body(self, machine):
        return [component.get_name() for component in machine.get_components_by_class(u.StaticMeshComponent)
                if component.get_name().startswith('MachinePart') and component.is_visible()]

    def audit_trooper_roster(self):
        rows = []
        for machine in self.machines():
            mesh, rifles = self.trooper_components(machine)
            asset = mesh.get_skeletal_mesh_asset() if mesh else None
            anim = mesh.get_anim_instance() if mesh else None
            capsule = machine.get_component_by_class(u.CapsuleComponent)
            attachments = []
            for rifle in rifles:
                mount, attached = self.rifle_bone_attachment(mesh, rifle)
                socket = str(mount.get_attach_socket_name()) if mount else 'NONE'
                attachments.append({'component': rifle.get_name(), 'socket': socket,
                    'mount': mount.get_name() if mount else None,
                    'mounted_on_trooper_mesh': attached,
                    'socket_exists': bool(mesh and mesh.does_socket_exist(socket))})
            rows.append({'stable_id': str(self.prop(machine, 'stable_id')),
                'kind': str(self.prop(machine, 'kind')),
                'mesh': asset.get_path_name() if asset else None,
                'anim_class': anim.get_class().get_name() if anim else None,
                'capsule_radius': float(capsule.get_scaled_capsule_radius()),
                'capsule_half_height': float(capsule.get_scaled_capsule_half_height()),
                'visible_legacy_body_parts': self.visible_legacy_body(machine),
                'rifles': attachments})
        self.report['trooper_roster_audit'] = rows
        expected = '/Game/SciFITrooper_Man_03/SkeletalMesh/SK_SciFITrooper_Man_03.'
        self.check('All thirty machines use the Sci-Fi Trooper skeletal mesh',
            len(rows) == 30 and all(r['mesh'] and r['mesh'].startswith(expected) for r in rows),
            [r['stable_id'] for r in rows if not r['mesh'] or not r['mesh'].startswith(expected)])
        self.check('All thirty troopers use the native rifle animation instance',
            all(r['anim_class'] == 'OBRifleAnimInstance' for r in rows),
            [{'id': r['stable_id'], 'anim': r['anim_class']} for r in rows if r['anim_class'] != 'OBRifleAnimInstance'])
        self.check('All standing trooper capsules fit the authored navigation envelope',
            all(0 < r['capsule_radius'] <= 44.01 and 85 <= r['capsule_half_height'] <= 112.01 for r in rows),
            [{'id': r['stable_id'], 'radius': r['capsule_radius'], 'half_height': r['capsule_half_height']} for r in rows])
        self.check('All trooper rifles attach to an existing skeletal weapon or hand bone',
            all(len(r['rifles']) == 1 and r['rifles'][0]['mounted_on_trooper_mesh'] and
                r['rifles'][0]['socket_exists'] and r['rifles'][0]['socket'] in ('weapon_r', 'hand_r') for r in rows),
            [{'id': r['stable_id'], 'rifles': r['rifles']} for r in rows])
        self.check('Legacy procedural enemy bodies are hidden on all thirty troopers',
            all(not r['visible_legacy_body_parts'] for r in rows),
            [{'id': r['stable_id'], 'visible_parts': r['visible_legacy_body_parts']} for r in rows if r['visible_legacy_body_parts']])
        self.event('Trooper asset and attachment preflight passed', roster_count=len(rows))

    def audit_active_trooper(self, machine, key):
        if key in self.trooper_active_seen:
            return
        mesh, rifles = self.trooper_components(machine)
        muzzle = machine.get_muzzle_location()
        origin = machine.get_actor_location()
        half = machine.get_component_by_class(u.CapsuleComponent).get_scaled_capsule_half_height()
        rifle = rifles[0] if len(rifles) == 1 else None
        mount, attached = self.rifle_bone_attachment(mesh, rifle) if rifle else (None, False)
        socket = str(mount.get_attach_socket_name()) if mount else 'NONE'
        location = rifle.get_world_location() if rifle else origin
        bone = mesh.get_socket_location(socket) if mesh and rifle else origin
        row = {'stable_id': str(self.prop(machine, 'stable_id')), 'kind': str(self.prop(machine, 'kind')),
            'position': self.xyz(origin), 'muzzle': self.xyz(muzzle),
            'muzzle_distance_cm': round(self.distance(origin, muzzle), 2),
            'muzzle_height_above_feet_cm': round(muzzle.z-origin.z+half, 2),
            'skeletal_mesh_visible': bool(mesh and mesh.is_visible()),
            'rifle_visible': bool(rifle and rifle.is_visible()),
            'rifle_mounted_on_trooper': attached,
            'rifle_socket': socket, 'rifle_position': self.xyz(location),
            'visible_legacy_body_parts': self.visible_legacy_body(machine),
            'rifle_attachment_offset_cm': round(self.distance(location, bone), 2)}
        self.report['trooper_active_audit'][key] = row
        valid_muzzle = all(math.isfinite(v) for v in (muzzle.x, muzzle.y, muzzle.z)) and \
            20 <= self.distance(origin, muzzle) <= 260 and 60 <= muzzle.z-origin.z+half <= 260
        self.check('Active trooper mesh, rifle and muzzle are valid: '+key,
            row['skeletal_mesh_visible'] and row['rifle_visible'] and
            row['rifle_mounted_on_trooper'] and not row['visible_legacy_body_parts'] and
            row['rifle_attachment_offset_cm'] <= 120 and valid_muzzle, row)
        self.trooper_active_seen.add(key)

    def controls(self):
        return {str(self.prop(a, 'stable_id')): a for a in
                u.GameplayStatics.get_all_actors_of_class(self.world, u.OBLevel2Interactable)}

    def flag(self, name):
        return bool(self.prop(self.director, name))

    def wait(self, label, condition, timeout=30.0, during=None):
        self.stage = label
        start = time.monotonic()
        while not condition():
            if time.monotonic()-start > timeout:
                raise TimeoutError(label + ' exceeded ' + str(timeout) + ' seconds')
            if during:
                during()
            yield

    def delay(self, seconds, label=None, during=None):
        until = time.monotonic()+seconds
        yield from self.wait(label or self.stage, lambda: time.monotonic() >= until,
                             seconds+10, during)

    def set_observation_health(self):
        self.player.set_editor_property('max_health', 10000.0)
        self.player.add_health(10000.0)
        self.event('Transient observation health enabled', health=10000)

    def navigation_call(self, native_name, *args):
        # UE 5.8's editor script-context hook asks the invocation object for its
        # world. Static Python navigation wrappers use a CDO whose Within=World
        # getter ensures. Bind these same native functions to the real PIE
        # navigation object instead; reacquire it after checkpoint map reload.
        nav = self.navigation_system
        if not nav or not u.SystemLibrary.is_valid(nav) or nav.get_outer() != self.world:
            systems = [obj for obj in u.ObjectIterator(u.NavigationSystemV1)
                       if obj.get_outer() == self.world]
            if len(systems) != 1:
                raise RuntimeError('Expected one navigation system owned by the current PIE world')
            self.navigation_system = nav = systems[0]
        return nav.call_method(native_name, args=(self.world, *args))

    def nav_point(self, position, capsule_center=False):
        # Query near walking-surface height, not the top of a console/cover.
        if capsule_center:
            half=self.player.get_component_by_class(u.CapsuleComponent).get_scaled_capsule_half_height()
            position=position-u.Vector(0,0,half)
        result = self.navigation_call('K2_ProjectPointToNavigation',
            position, None, None, u.Vector(90,90,80))
        if hasattr(result, 'x'):
            return result
        if isinstance(result, tuple):
            if any(r is False for r in result):
                return None
            return next((r for r in result if hasattr(r, 'x')), None)
        return None

    def query_nav_path(self, target):
        start=self.player.get_actor_location()
        half=self.player.get_component_by_class(u.CapsuleComponent).get_scaled_capsule_half_height()
        a, b = self.nav_point(start,True), self.nav_point(target,True)
        path = self.navigation_call('FindPathToLocationSynchronously',
            a, b, None, None) if a is not None and b is not None else None
        valid=bool(path and path.is_valid())
        partial=bool(path and path.is_partial())
        self.last_nav_diagnostic={'from':self.xyz(start),'to':self.xyz(target),
            'source_floor_query':self.xyz(start-u.Vector(0,0,half)),
            'target_floor_query':self.xyz(target-u.Vector(0,0,half)),
            'query_extent':[90,90,80],
            'projected_start':self.xyz(a) if a is not None else None,
            'projected_target':self.xyz(b) if b is not None else None,
            'path_valid':valid,'path_partial':partial,
            'path_points':[self.xyz(p) for p in path.get_editor_property('path_points')] if path else []}
        return path if valid and not partial else None

    def wait_for_gate_navigation(self, target, zone):
        started=time.monotonic()
        while time.monotonic()-started<6.0:
            if self.query_nav_path(target):
                self.event('Runtime navigation ready after gate',zone=zone,
                           waited_seconds=round(time.monotonic()-started,2))
                return
            yield from self.delay(.25,'Waiting for T%02d gate navigation'%zone)
        self.report['navigation'].append({'label':'Failed opened gate to zone '+str(zone),
            'complete_path':False,**self.last_nav_diagnostic})
        self.check('Opened gate has a complete runtime route to zone '+str(zone),False,self.last_nav_diagnostic)

    def nav_probe(self, target, label):
        path = self.query_nav_path(target)
        valid = bool(path)
        self.report['navigation'].append({'label': label,**self.last_nav_diagnostic,
            'complete_path': valid,'length_cm': round(path.get_path_length(),2) if valid else None})
        return path if valid else None

    def teleport(self, position, label, project=True):
        old = self.player.get_actor_location()
        desired = position
        if project:
            nav = self.nav_point(position,True)
            if nav is not None:
                capsule = self.player.get_component_by_class(u.CapsuleComponent)
                desired = u.Vector(nav.x, nav.y, nav.z+capsule.get_scaled_capsule_half_height()+3)
        self.player.cancel_actions()
        self.player.get_component_by_class(u.CharacterMovementComponent).stop_movement_immediately()
        self.player.set_actor_location(desired, False, True)
        self.report['teleports'].append({'label': label, 'run': self.current_run,
            'from': self.xyz(old), 'to': self.xyz(self.player.get_actor_location())})

    def look_at(self, position):
        origin = self.player.get_actor_location()+u.Vector(0,0,55)
        delta = position-origin
        yaw = math.degrees(math.atan2(delta.y,delta.x))
        pitch = math.degrees(math.atan2(delta.z,math.hypot(delta.x,delta.y)))
        self.controller.set_control_rotation(u.Rotator(pitch=pitch,yaw=yaw,roll=0))
        self.player.set_actor_rotation(u.Rotator(pitch=0,yaw=yaw,roll=0),False)

    def walk_route(self, target, label, timeout=45):
        path = self.nav_probe(target,label)
        self.check('Complete navigation path for '+label,path is not None,self.last_nav_diagnostic)
        points = list(path.get_editor_property('path_points'))
        started = time.monotonic()
        actual_distance=0.0
        previous=self.player.get_actor_location()
        self.stage = 'Input walk: '+label
        for point in points[1:]:
            best_distance=float('inf')
            last_progress=time.monotonic()
            while True:
                now=time.monotonic()
                here=self.player.get_actor_location()
                distance=math.hypot(point.x-here.x,point.y-here.y)
                actual_distance+=math.hypot(here.x-previous.x,here.y-previous.y)
                previous=here
                if distance<=70:break
                if distance<best_distance-12:
                    best_distance=distance;last_progress=now
                if now-started>timeout or now-last_progress>5:
                    self.event('Movement route blocked',label=label,position=self.xyz(here),
                               target_node=self.xyz(point),distance_to_node=round(distance,2),
                               elapsed_seconds=round(now-started,2),actual_distance_cm=round(actual_distance,2))
                    raise RuntimeError('Character failed to traverse '+label+'; no teleport fallback permitted')
                self.look_at(point+u.Vector(0,0,60))
                direction=self.direction(here,u.Vector(point.x,point.y,here.z))
                frame=max(.03,min(1.0,u.GameplayStatics.get_world_delta_seconds(self.world)))
                speed=max(1.0,float(self.prop(self.player,'walk_speed')))
                # Braking near nodes prevents low-frame-rate overshoot/oscillation.
                scale=max(.1,min(1.0,distance/(speed*frame*2.5)))
                self.player.add_movement_input(direction,scale,True)
                yield
            self.player.get_component_by_class(u.CharacterMovementComponent).stop_movement_immediately()
        self.event('Input walk completed',label=label,path_distance_cm=round(path.get_path_length(),2),
                   actual_distance_cm=round(actual_distance,2),seconds=round(time.monotonic()-started,2),
                   final_position=self.xyz(self.player.get_actor_location()))

    def walk_initial_route(self):
        yield from self.walk_route(u.Vector(1800,0,110),'Crash chamber to maintenance entry',35)

    def enter_zone(self, number, position):
        if number>=4:
            yield from self.wait_for_gate_navigation(position,number)
            yield from self.walk_route(position,'Opened gate to zone '+str(number),45)
        else:
            self.nav_probe(position, 'Route to zone ' + str(number))
            self.teleport(position,'Enter actual T%02d trigger'%number)
        yield from self.wait('Waiting for T%02d overlap'%number,
            lambda: int(self.prop(self.director,'current_zone'))>=number,15)
        self.event('Zone trigger verified',zone=number)

    def observe_and_damage(self, encounters=None, kill=True):
        now=time.monotonic()
        alive_finale=0; attacking=0
        for machine in self.machines():
            if not u.SystemLibrary.is_valid(machine) or machine.is_dead() or not self.prop(machine,'active'):
                continue
            eid=int(self.prop(machine,'encounter_id'))
            if 210<=eid<=213: alive_finale+=1
            state=self.prop(machine,'state')
            if state in (u.OBMachineState.WINDUP,u.OBMachineState.LUNGE,u.OBMachineState.BURST,u.OBMachineState.PULSE_WARNING):
                attacking+=1
            if encounters is not None and eid not in encounters: continue
            name=str(self.prop(machine,'stable_id'))
            key=self.current_run+':'+name
            first=self.active_seen.setdefault(key,now)
            if now-first >= .25:
                self.audit_active_trooper(machine,key)
            sample=self.states_seen.setdefault(key,{'encounter':eid,'kind':str(self.prop(machine,'kind')),
                'first_position':self.xyz(machine.get_actor_location()),'states':[], 'damage_injected':0})
            state_text=str(state)
            if state_text not in sample['states']:sample['states'].append(state_text)
            sample['last_position']=self.xyz(machine.get_actor_location())
            sample['observed_seconds']=round(now-first,2)
            attack_phase=state in (u.OBMachineState.WINDUP,u.OBMachineState.LUNGE,u.OBMachineState.BURST,u.OBMachineState.PULSE_WARNING)
            if attack_phase:sample.setdefault('first_attack_seconds',round(now-first,2))
            delay=10.0 if self.prop(machine,'kind')==u.OBMachineKind.WARDEN else 3.0
            announced=(eid==205 or 210<=eid<=213)
            if announced:
                # Let the reinforcement actually navigate out and telegraph before
                # scripted damage, so distant idle enemies cannot masquerade as
                # tested combat. Eighteen seconds is a bounded diagnostic fallback.
                delay=sample.get('first_attack_seconds',17.0)+1.0
                delay=max(3.0,min(delay,18.0))
            if kill and now-first>=delay and now-self.last_damage.get(key,-100)>0.25:
                if announced and 'first_attack_seconds' not in sample:
                    warning='Announced reinforcement never reached an attack phase before diagnostic damage: '+key
                    if warning not in self.report['warnings']:self.report['warnings'].append(warning)
                self.last_damage[key]=now
                direction=self.direction(self.player.get_actor_location(),machine.get_actor_location())
                applied=u.GameplayStatics.apply_point_damage(machine,10000.0,direction,u.HitResult(),self.controller,self.player,u.DamageType)
                sample['damage_injected']+=float(applied)
        run=self.report['runs'].setdefault(self.current_run,{})
        run['max_finale_alive']=max(run.get('max_finale_alive',0),alive_finale)
        run['max_observed_attackers']=max(run.get('max_observed_attackers',0),attacking)
        if alive_finale>5: raise RuntimeError('Finale alive cap exceeded: '+str(alive_finale))
        if attacking>4: raise RuntimeError('Attack slot cap exceeded: '+str(attacking))

    def fight(self, ids, label, timeout=70):
        wanted=set(ids)
        yield from self.wait(label,
            lambda: all(self.director.is_encounter_cleared(i) for i in ids),
            timeout,lambda:self.observe_and_damage(wanted))
        self.event('Native encounter cleared',encounters=ids)

    def interact(self, stable_id, repair=False):
        controls=self.controls()
        self.check('Control exists: '+stable_id,stable_id in controls)
        control=controls[stable_id]
        target=control.get_actor_location()+u.Vector(0,0,65)
        center=control.get_actor_location()
        forward=control.get_actor_forward_vector()
        stand=None
        for offset in (0,90,-90,180,45,-45,135,-135):
            angle=math.radians(offset)
            dx=-forward.x*math.cos(angle)+forward.y*math.sin(angle)
            dy=-forward.x*math.sin(angle)-forward.y*math.cos(angle)
            candidate=center+u.Vector(dx*180,dy*180,0)
            nav=self.nav_point(candidate)
            if nav is None:continue
            horizontal=math.hypot(nav.x-center.x,nav.y-center.y)
            if horizontal<115 or horizontal>215:continue
            # Ensure the requested native contextual action is the nearest.
            if any(other!=control and math.hypot(other.get_actor_location().x-nav.x,other.get_actor_location().y-nav.y)<horizontal+15
                   for other in controls.values()):continue
            half=self.player.get_component_by_class(u.CapsuleComponent).get_scaled_capsule_half_height()
            stand=u.Vector(nav.x,nav.y,nav.z+half+3);break
        if stand is None:raise RuntimeError('No unambiguous native interaction position: '+stable_id)
        self.teleport(stand,'Approach control '+stable_id)
        self.look_at(target)
        yield from self.delay(.7,'Settling beside '+stable_id)
        duration=2.0 if repair else float(self.prop(control,'hold_seconds'))
        self.stage='Interact '+stable_id
        # Keep real world time and damage cancellation. Only the pawn input
        # watchdog is suspended while synthetic hold input is being supplied.
        self.hold_player=self.player
        self.player.set_actor_tick_enabled(False)
        started=time.monotonic(); attempts=0; interruptions=[]; attempted_hold=False; last_attempt_damage=-100.0
        try:
            while True:
                self.look_at(target)
                if not self.prop(self.player,'interaction_active'):
                    if attempted_hold:
                        damage_time=float(self.prop(self.player,'last_damage_time'))
                        interruptions.append({'elapsed':round(time.monotonic()-started,2),
                            'reason':'damage' if damage_time>last_attempt_damage else 'native hold cancelled (range/state/focus)',
                            'last_damage_time':damage_time,'health':float(self.prop(self.player,'health')),
                            'position':self.xyz(self.player.get_actor_location())})
                    attempted_hold=False
                    self.player.gamepad_context_released()
                    self.player.gamepad_context_pressed()
                    attempts+=1
                    active=bool(self.prop(self.player,'interaction_active'))
                    attempted_hold=active
                    last_attempt_damage=float(self.prop(self.player,'last_damage_time'))
                    if duration<=0:
                        if stable_id=='L2_CrashSupplies':
                            done=self.player.get_arc_reserve_ammo()>=96
                        else:
                            done=self.director.is_action_complete(self.prop(control,'action'))
                        if done:break
                    if not active:
                        if time.monotonic()-started>8:
                            self.event('Native contextual interaction rejected',control=stable_id,attempts=attempts,
                                       interruptions=interruptions,player_position=self.xyz(self.player.get_actor_location()),
                                       gameplay_active=self.player.is_gameplay_active(),prompt=str(self.prop(self.player,'interaction_prompt')))
                            raise RuntimeError('Native contextual interaction rejected: '+stable_id+' after '+str(attempts)+' attempts')
                        yield
                        continue
                    # The native HoldTarget is not exported to Python. UpdateHold
                    # validates that this supplied control equals that target;
                    # mismatches cancel, and completion is checked below.
                self.director.update_hold(self.player,control,max(0.0,u.GameplayStatics.get_world_delta_seconds(self.world)))
                if not self.prop(self.player,'interaction_active'):
                    # On damage the native hold cancels without completing; retry.
                    done=self.flag('overload_repaired') if repair else self.director.is_action_complete(self.prop(control,'action'))
                    if done:break
                if time.monotonic()-started>25:
                    self.event('Timed hold failed',control=stable_id,attempts=attempts,interruptions=interruptions,
                               progress=float(self.prop(self.player,'interaction_progress')),
                               player_position=self.xyz(self.player.get_actor_location()),
                               gameplay_active=self.player.is_gameplay_active(),prompt=str(self.prop(self.player,'interaction_prompt')))
                    raise TimeoutError('Timed hold failed: '+stable_id+' after '+str(attempts)+' attempts; '+str(len(interruptions))+' interruptions')
                yield
        finally:
            if self.hold_player and u.SystemLibrary.is_valid(self.hold_player):
                self.hold_player.gamepad_context_released()
                self.hold_player.set_actor_tick_enabled(True)
                self.hold_player.cancel_actions()
            self.hold_player=None
        self.event('Native interaction completed',control=stable_id,hold_seconds=duration,
                   elapsed_seconds=round(time.monotonic()-started,2),attempts=attempts,interruptions=interruptions,repair=repair)
        yield from self.delay(.4,'After '+stable_id)

    def water_test(self, live):
        self.teleport(u.Vector(6600,3000,105),'Electrical water '+('live' if live else 'isolated'))
        self.player.end_spawn_grace()
        yield from self.delay(.35,'Settle in water')
        before=float(self.prop(self.player,'health'))
        yield from self.delay(1.2,'Observe electrical water')
        lost=before-float(self.prop(self.player,'health'))
        self.check('Live water damages' if live else 'Isolated water is harmless',lost>=6 if live else lost==0,{'health_lost':lost,'seconds':1.2})
        self.event('Water hazard measured',live=live,health_lost=lost)

    def defense_position(self):
        self.teleport(u.Vector(19300,9500,110),'Defense observation position')

    def defend(self):
        self.observe_and_damage(set([210,211,212,213]))
        self.controller.set_control_rotation(u.Rotator(pitch=-8,yaw=(time.monotonic()*35)%360,roll=0))

    def finale(self, retry=False):
        yield from self.interact('L2_StabilizerB')
        self.check('B-first stabilizer activation',self.flag('stabilizer_b') and not self.flag('stabilizer_a'))
        # Clear the fight triggered by the first stabilizer before attempting
        # another stationary hold. Damage must keep interrupting real holds.
        self.defense_position()
        yield from self.fight([210],'Opening stabilizer defense before second activation',90)
        yield from self.interact('L2_StabilizerA')
        self.check('Both stabilizers online after opening defense',self.flag('stabilizer_a') and self.flag('stabilizer_b'))
        yield from self.interact('L2_Transmitter')
        self.defense_position()
        yield from self.wait('Defend transmission to overload',lambda:self.director.is_uplink_paused(),80,self.defend)
        self.check('Overload occurs at fifty seconds',abs(float(self.prop(self.director,'uplink_seconds'))-50)<.01)
        yield from self.delay(3.0,'Verify paused uplink',self.defend)
        self.check('Overload progress remains at fifty',abs(float(self.prop(self.director,'uplink_seconds'))-50)<.01)
        yield from self.interact('L2_StabilizerA',repair=True)
        self.check('Repair resumes without restart',self.flag('overload_repaired') and float(self.prop(self.director,'uplink_seconds'))>=50)
        if retry:
            self.event('Testing death after overload repair',checkpoint=int(self.prop(self.director,'checkpoint')))
            self.player.end_spawn_grace()
            self.player.damage_player(float(self.prop(self.player,'health'))+1,self.player.get_actor_location())
            self.check('Actual native death occurred',self.prop(self.player,'dead'))
            old_player=self.player
            self.director.retry_checkpoint()
            yield from self.wait('Reload finale checkpoint',lambda:self.player and self.player!=old_player and self.director and self.player.is_gameplay_active(),55)
            self.check('Retry remains isolated development session',self.prop(self.gi,'development_chapter_session'))
            self.check('Retry restores finale checkpoint',int(self.prop(self.director,'checkpoint'))==3)
            self.check('Retry skips crash cinematic',not self.director.is_cinematic_active())
            self.check('Retry preserves power and Warden key',self.flag('aux_power_restored') and self.flag('warden_defeated'))
            self.check('Retry clears both stabilizers and overload',not any(self.flag(x) for x in ['stabilizer_a','stabilizer_b','overload_triggered','overload_repaired']))
            self.check('Retry resets transmission and finale count',float(self.prop(self.director,'uplink_seconds'))==0 and int(self.prop(self.director,'finale_spawned'))==0)
            self.check('Retry restores standard player health',float(self.prop(self.player,'health'))==float(self.prop(self.player,'max_health'))==100)
            self.check('Retry leaves finale enemies dormant',not any(self.prop(m,'active') for m in self.machines() if int(self.prop(m,'encounter_id'))>=210))
            self.event('Actual death and map reload verified')
            self.current_run='completion_after_retry'
            self.set_observation_health()
            yield from self.delay(4.5,'Checkpoint spawn grace')
            return
        self.defense_position()
        yield from self.wait('Finish finite transmission defense',lambda:self.director.is_cinematic_active(),75,self.defend)
        self.check('Finite finale activates exactly thirteen enemies',int(self.prop(self.director,'finale_spawned'))==13)
        self.check('Finale hostiles are all resolved',all(self.director.is_encounter_cleared(i) for i in [210,211,212,213]))
        ending_start=time.monotonic()
        ending_start_epoch=time.time()
        self.event('Rescue response cinematic began')
        yield from self.wait('Watch rescue response without skipping',lambda:not self.director.is_cinematic_active(),30,lambda:self.observe_camera('ending'))
        self.report['ending_watched_seconds']=round(time.monotonic()-ending_start,2)
        self.check('Completion screen is shown',str(self.prop(self.controller,'menu_screen'))=='Complete')
        self.check('Native game instance retains a valid completed-session save',self.gi.has_valid_save())
        development_save=self.saved/'SaveGames'/'OB_DeadSignal_Development.sav'
        self.check('Development save file exists',development_save.exists())
        stat=development_save.stat()
        self.check('Development save was written after the rescue response began',stat.st_mtime>=ending_start_epoch-.1)
        self.report['completed_development_save']={'path':str(development_save),'bytes':stat.st_size,
            'sha256':hashlib.sha256(development_save.read_bytes()).hexdigest(),
            'modified_epoch':stat.st_mtime,
            'scope':'Completion screen, native HasValidSave, and fresh development-file write; private save fields are not inspected'}
        self.report['runs'][self.current_run]['finale_spawned']=int(self.prop(self.director,'finale_spawned'))
        self.event('Native level completion verified')

    def observe_camera(self, label):
        target=self.controller.get_view_target()
        name=target.get_name() if target else 'NONE'
        seen=self.report.setdefault(label+'_camera_targets',[])
        if name not in seen:seen.append(name)

    def preflight_bindings(self):
        # Fail before the long mission if any public binding is unavailable.
        # UPROPERTY() alone is protected from Python; use only Edit/Blueprint
        # visible properties or public reflected methods.
        for name in ('current_zone','checkpoint','machines','junction_drained','aux_power_restored',
                     'warden_defeated','stabilizer_a','stabilizer_b','overload_triggered',
                     'overload_repaired','uplink_seconds','finale_spawned'):
            self.prop(self.director,name)
        for name in ('health','max_health','dead','interaction_active','last_damage_time','interaction_prompt','interaction_progress','walk_speed'):
            self.prop(self.player,name)
        self.prop(self.controller,'menu_screen')
        self.prop(self.gi,'development_chapter_session')
        for control in self.controls().values():
            for name in ('stable_id','hold_seconds','action'):self.prop(control,name)
        for machine in self.machines():
            for name in ('active','encounter_id','stable_id','kind','state'):self.prop(machine,name)
            if not callable(getattr(machine,'get_muzzle_location',None)):
                raise RuntimeError('Missing public trooper muzzle binding: '+str(self.prop(machine,'stable_id')))
        for obj,names in ((self.player,('gamepad_context_pressed','gamepad_context_released','end_spawn_grace','damage_player')),
                          (self.director,('update_hold','retry_checkpoint','is_action_complete','is_encounter_cleared')),
                          (self.gi,('has_valid_save',))):
            for name in names:self.check('Public runtime binding '+name,callable(getattr(obj,name,None)))
        self.check('World delta binding for native hold timing',u.GameplayStatics.get_world_delta_seconds(self.world)>=0)
        self.event('Public binding preflight passed')

    def run(self):
        yield from self.wait('Start isolated Level2 PIE',lambda:self.player and self.director and self.gi and (self.director.is_cinematic_active() or self.player.is_gameplay_active()),70)
        self.check('Direct map uses development session',self.prop(self.gi,'development_chapter_session'))
        self.preflight_bindings()
        self.audit_trooper_roster()
        self.check('Player is Wraith', 'Wraith' in self.player.get_component_by_class(u.SkeletalMeshComponent).get_skeletal_mesh_asset().get_path_name())
        self.report['player_mesh']=self.player.get_component_by_class(u.SkeletalMeshComponent).get_skeletal_mesh_asset().get_path_name()
        self.report['map']=self.world.get_path_name()
        self.report['machine_roster_count']=len(self.machines())
        self.check('Complete authored enemy roster',len(self.machines())==30)
        self.check('Crash cinematic starts',self.director.is_cinematic_active())
        intro_start=time.monotonic()
        self.event('Crash cinematic began')
        yield from self.wait('Watch full crash without skipping',lambda:not self.director.is_cinematic_active() and self.player.is_gameplay_active(),75,lambda:self.observe_camera('opening'))
        self.report['opening_watched_seconds']=round(time.monotonic()-intro_start,2)
        self.check('Crash finalizer establishes checkpoint zero',int(self.prop(self.director,'checkpoint'))==0)
        self.check('Crash finalizer restores health',self.prop(self.player,'health')==self.prop(self.player,'max_health'))
        self.set_observation_health()
        yield from self.delay(6.2,'Post-crash safety interval')
        yield from self.interact('L2_CrashSupplies')
        yield from self.walk_initial_route()
        yield from self.wait('Maintenance trigger activates',lambda:int(self.prop(self.director,'current_zone'))>=2,12)
        self.teleport(u.Vector(2350,0,110),'Observe first Cutter approach')
        yield from self.fight([201],'Cutter introduction')
        self.teleport(u.Vector(4700,650,110),'Observe Watcher introduction')
        yield from self.fight([202],'Watcher introduction')
        yield from self.enter_zone(3,u.Vector(5000,2150,110))
        self.teleport(u.Vector(6200,3500,110),'Junction dry walkway combat')
        yield from self.fight([203],'Junction security')
        yield from self.water_test(True)
        yield from self.interact('L2_CableBreaker')
        yield from self.water_test(False)
        yield from self.interact('L2_DrainPump')
        self.check('Junction checkpoint committed',int(self.prop(self.director,'checkpoint'))==1 and self.flag('junction_drained'))
        yield from self.enter_zone(4,u.Vector(8850,3000,110))
        self.teleport(u.Vector(9700,3000,110),'Freight combat observation')
        yield from self.fight([204,205],'Freight and Bulwark reinforcement')
        yield from self.enter_zone(5,u.Vector(12200,5900,110))
        self.teleport(u.Vector(12900,6900,110),'Auxiliary combat observation')
        yield from self.fight([206],'Auxiliary security')
        yield from self.interact('L2_CouplerCabinet')
        yield from self.interact('L2_CouplerSocket')
        yield from self.interact('L2_AuxRestart')
        self.check('Power checkpoint committed',int(self.prop(self.director,'checkpoint'))==2 and self.flag('aux_power_restored'))
        yield from self.enter_zone(6,u.Vector(14750,7000,110))
        self.teleport(u.Vector(15000,7000,110),'Observe first pressure vent')
        self.player.end_spawn_grace()
        before=float(self.prop(self.player,'health'))
        yield from self.delay(5.0,'Observe complete vent cycle')
        lost=before-float(self.prop(self.player,'health'))
        self.check('Pressure vent deals bounded contact damage',15<=lost<=60,{'health_lost':lost,'seconds':5})
        self.teleport(u.Vector(17000,7000,110),'Observe isolated Relay Warden')
        yield from self.fight([207],'Relay Warden telegraphs and defeat',80)
        self.check('Warden grants key and finale checkpoint',self.flag('warden_defeated') and int(self.prop(self.director,'checkpoint'))==3)
        yield from self.enter_zone(7,u.Vector(18620,9500,110))
        self.current_run='retry_probe'
        yield from self.finale(retry=True)
        yield from self.finale(retry=False)
        audited_ids={row['stable_id'] for row in self.report['trooper_active_audit'].values()}
        self.check('All thirty authored troopers were checked while active',len(audited_ids)==30,sorted(audited_ids))
        self.check('Campaign save SHA-256 unchanged',self.hash_saves()==self.report['campaign_hash_before'])
        self.finish(True)

    def finish(self, success, error=None):
        if self.handle is not None:
            u.unregister_slate_post_tick_callback(self.handle); self.handle=None
        if self.hold_player and u.SystemLibrary.is_valid(self.hold_player):
            self.hold_player.set_actor_tick_enabled(True)
            self.hold_player.cancel_actions()
        if self.player and u.SystemLibrary.is_valid(self.player):
            self.player.set_editor_property('max_health',100.0)
        if success:
            # A functional mission pass does not imply AI attacks were observed.
            # Record missing evidence instead of silently treating injected kills
            # as proof of navigation/combat presentation.
            by_kind={}
            for observation in self.states_seen.values():
                by_kind.setdefault(observation['kind'],set()).update(observation['states'])
            self.report['observed_enemy_states_by_kind']={k:sorted(v) for k,v in by_kind.items()}
            for kind,states in by_kind.items():
                if not any(any(cue in state for cue in ('WINDUP','LUNGE','BURST','PULSE_WARNING')) for state in states):
                    self.report['warnings'].append('No live attack phase observed for '+kind+'; scripted damage is not combat validation')
        self.report['campaign_hash_after']=self.hash_saves()
        if self.report['campaign_hash_after']!=self.report['campaign_hash_before']:
            success=False
            self.report['errors'].append('Campaign save changed unexpectedly; smoke did not restore or edit that file')
        if error:self.report['errors'].append(error)
        for label,samples in self.frames.items():
            if not samples:continue
            ordered=sorted(samples)
            self.report['performance'][label]={'frames':len(samples),'mean_fps_proxy':round(len(samples)/sum(samples),2),
                'p95_frame_ms':round(ordered[min(len(ordered)-1,int(len(ordered)*.95))]*1000,2),
                'scope':'Editor Slate tick timing; includes editor overhead, not GPU profiling'}
        self.report['status']=('PASS_WITH_LIMITATIONS' if self.report['warnings'] else 'PASS') if success else 'FAIL'
        self.report['completed_utc']=datetime.now(timezone.utc).isoformat()
        self.write()
        u.log('OB_LEVEL2_SMOKE_FINISHED '+self.report['status']+' '+str(self.out))
        if self.editor.get_game_world():self.levels.editor_request_end_play()

    def tick(self, dt):
        try:
            self.dt=max(0.0,float(dt))
            self.frames.setdefault(self.stage,[]).append(max(.0001,self.dt))
            if time.monotonic()-self.started>900:raise TimeoutError('Total runtime smoke exceeded fifteen minutes')
            self.refresh()
            next(self.steps)
            if time.monotonic()-self.last_write>1:self.write()
        except StopIteration:
            if self.handle is not None:self.finish(True)
        except Exception:
            self.finish(False,traceback.format_exc())

    def start(self):
        if self.editor.get_game_world():
            raise RuntimeError('Stop the existing PIE session before running the smoke test')
        editor_world=self.editor.get_editor_world()
        if not editor_world or 'L_DeadSignal' not in editor_world.get_name():
            raise RuntimeError('Load saved L_DeadSignal before running the smoke test')
        self.report['status']='RUNNING';self.write()
        self.steps=self.run()
        self.handle=u.register_slate_post_tick_callback(self.tick)
        self.levels.editor_request_begin_play()
        u.log('OB_LEVEL2_SMOKE_STARTED '+str(self.out))


# Keep a strong reference for the callback lifetime. Re-running while active is
# rejected instead of creating two drivers that can both mutate PIE.
import builtins
_previous=getattr(builtins,'_orbit_dead_signal_smoke',None)
if _previous and getattr(_previous,'handle',None) is not None:
    raise RuntimeError('A Dead Signal smoke test is already running')
_smoke=DeadSignalSmoke()
builtins._orbit_dead_signal_smoke=_smoke
_smoke.start()
