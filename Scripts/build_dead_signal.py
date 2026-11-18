"""Author Orbit Breakout Chapter 2 using installed Underground Tunnel assets.

Run in the full Unreal Editor after building the native module. Creates/saves a
real map and two editable Level Sequences; only OB_L2_GENERATED actors are owned.
Vendor content and Chapter 1 are never rewritten. Reruns back up this generated
map/sequences and preserve untagged owner actors. Navigation verification finishes
on editor ticks; read Saved/Level2/BUILD_RESULT.json for the final result.
"""
import json
import math
import shutil
import sys
import time
import traceback
from datetime import datetime, timezone
from pathlib import Path
import unreal as u

ROOT = Path(u.Paths.project_dir()).resolve()
BASE = '/Game/OrbitBreakout/Level2'
MAP = '/Game/OrbitBreakout/Maps/L_DeadSignal'
TAG = 'OB_L2_GENERATED'
KIT = '/Game/UndergroundTunnel'
ACTORS = u.get_editor_subsystem(u.EditorActorSubsystem)
LEVELS = u.get_editor_subsystem(u.LevelEditorSubsystem)
ASSETS = u.AssetToolsHelpers.get_asset_tools()
MATS, MESHES, COUNTS, ANCHORS, INTERACTIONS, MACHINES = {}, {}, {}, [], [], []
REPORT = {'map': MAP, 'started_utc': datetime.now(timezone.utc).isoformat(), 'status': 'BUILDING'}
TUNNEL_AXIS = 'X'
_NAV = None


def v(a):
    return u.Vector(*a)


def native(name):
    c = u.load_class(None, '/Script/OrbitBreakout.' + name)
    if not c:
        raise RuntimeError('Build the native module first: ' + name)
    return c


def asset(path):
    if path not in MESHES:
        MESHES[path] = u.load_asset(path)
    if not MESHES[path]:
        raise RuntimeError('Required installed asset is missing: ' + path)
    return MESHES[path]


def own(a, name, folder='Architecture', tags=()):
    if not a:
        raise RuntimeError('Failed to create ' + name)
    a.set_actor_label(name)
    owned_tags=list(a.tags)
    for tag in [TAG]+list(tags):
        if u.Name(tag) not in owned_tags: owned_tags.append(u.Name(tag))
    a.set_editor_property('tags',owned_tags)
    a.set_folder_path(u.Name('DeadSignal/' + folder))
    COUNTS[folder] = COUNTS.get(folder, 0) + 1
    return a


def spawn(c, name, p, yaw=0, folder='Gameplay', tags=()):
    return own(ACTORS.spawn_actor_from_class(c, v(p), u.Rotator(pitch=0, yaw=yaw, roll=0)), name, folder, tags)


def bounds(mesh):
    b = mesh.get_bounds()
    return b.origin, b.box_extent


def fitted(name, path, center, size=None, yaw=0, collision=False, folder='Architecture', tags=(), movable=False):
    a = spawn(u.StaticMeshActor, name, center, yaw, folder, tags)
    c = a.static_mesh_component
    m = asset(path)
    c.set_static_mesh(m)
    c.set_mobility(u.ComponentMobility.MOVABLE if movable else u.ComponentMobility.STATIC)
    c.set_collision_profile_name('BlockAll' if collision else 'NoCollision')
    c.set_editor_property('cast_shadow', True)
    o, e = bounds(m)
    sc = tuple(size[i] / max(1., (e.x, e.y, e.z)[i] * 2.) for i in range(3)) if size else (1., 1., 1.)
    a.set_actor_scale3d(v(sc))
    rad = math.radians(yaw)
    ox, oy = o.x * sc[0], o.y * sc[1]
    a.set_actor_location(v((center[0] - ox * math.cos(rad) + oy * math.sin(rad), center[1] - ox * math.sin(rad) - oy * math.cos(rad), center[2] - o.z * sc[2])), False, False)
    return a


def simple_material(name, color, emission=0., rough=.6, metallic=.2):
    path = BASE + '/Materials/' + name
    m = u.load_asset(path)
    if m:
        return m
    m = ASSETS.create_asset(name, BASE + '/Materials', u.Material, u.MaterialFactoryNew())
    edit = u.MaterialEditingLibrary
    col = edit.create_material_expression(m, u.MaterialExpressionVectorParameter, -350, 0)
    col.set_editor_property('parameter_name', 'Color')
    col.set_editor_property('default_value', u.LinearColor(*color, 1))
    edit.connect_material_property(col, '', u.MaterialProperty.MP_BASE_COLOR)
    for title, value, prop, yy in [('Roughness', rough, u.MaterialProperty.MP_ROUGHNESS, 140), ('Metallic', metallic, u.MaterialProperty.MP_METALLIC, 240)]:
        n = edit.create_material_expression(m, u.MaterialExpressionScalarParameter, -350, yy)
        n.set_editor_property('parameter_name', title)
        n.set_editor_property('default_value', value)
        edit.connect_material_property(n, '', prop)
    if emission:
        n = edit.create_material_expression(m, u.MaterialExpressionScalarParameter, -350, -140)
        n.set_editor_property('parameter_name', 'EmissiveStrength')
        n.set_editor_property('default_value', emission)
        mult = edit.create_material_expression(m, u.MaterialExpressionMultiply, -130, -100)
        edit.connect_material_expressions(col, '', mult, 'A')
        edit.connect_material_expressions(n, '', mult, 'B')
        edit.connect_material_property(mult, '', u.MaterialProperty.MP_EMISSIVE_COLOR)
    edit.set_base_material_usage(m, u.MaterialUsage.MATUSAGE_NANITE, True)
    edit.recompile_material(m)
    u.EditorAssetLibrary.save_loaded_asset(m, False)
    return m


def materials():
    MATS['concrete'] = asset(KIT + '/Materials/MI_Env_ConcreteFloor01a')
    MATS['metal'] = asset(KIT + '/Materials/MI_MetalPanel01a')
    for key, color, glow in [('dark', (.024,.032,.037),0), ('copper',(.28,.11,.045),0), ('red',(.85,.025,.012),1.7), ('amber',(.85,.3,.025),1.7), ('teal',(.015,.35,.48),1.6), ('green',(.02,.55,.2),1.4), ('white',(.6,.72,.8),1.2), ('planet',(.025,.085,.15),.2)]:
        MATS[key] = simple_material('M_L2_' + key, color, glow)


def box(name, p, size, mat='concrete', collision=True, yaw=0, folder='Architecture', tags=()):
    a = fitted(name, '/Engine/BasicShapes/Cube', p, size, yaw, collision, folder, tags)
    a.static_mesh_component.set_material(0, MATS[mat])
    return a


def prop(name, mesh, p, size=None, yaw=0, collision=False, tags=(), folder='Props', movable=False):
    return fitted(name, KIT + '/Meshes/Props/' + mesh, p, size, yaw, collision, folder, tags, movable)


def text(name, words, p, yaw=180, size=28, color=(170,220,230)):
    a = spawn(u.TextRenderActor, name, p, yaw, 'Signage')
    c = a.text_render
    c.set_text(words)
    c.set_world_size(size)
    c.set_text_render_color(u.Color(r=color[0], g=color[1], b=color[2], a=255))
    c.set_horizontal_alignment(u.HorizTextAligment.EHTA_CENTER)
    c.set_vertical_alignment(u.VerticalTextAligment.EVRTA_TEXT_CENTER)
    return a


def light(name, p, color=(.45,.65,.8), radius=1300, intensity=18000, shadows=False, power=False):
    a = spawn(u.PointLight, name, p, folder='Lighting', tags=('L2PowerLight',) if power else ())
    c = a.point_light_component
    c.set_mobility(u.ComponentMobility.MOVABLE)
    c.set_editor_property('intensity_units', u.LightUnits.LUMENS)
    c.set_intensity(intensity)
    c.set_light_color(u.LinearColor(*color, 1))
    c.set_attenuation_radius(radius)
    c.set_cast_shadows(shadows)
    c.set_source_radius(30.)
    return a


def bulkhead(name, x, y, width, height=760, yaw=0, door_id=None):
    rad=math.radians(yaw)
    def at(dx,dy,dz):
        return (x+dx*math.cos(rad)-dy*math.sin(rad), y+dx*math.sin(rad)+dy*math.cos(rad),dz)
    opening=650
    side=(width-opening)*.5
    for sign in [-1,1]:
        box(name+'_Pier'+str(sign),at(0,sign*(opening/2+side/2),height/2),(90,side,height),'concrete',True,yaw)
        box(name+'_SteelJamb'+str(sign),at(-52,sign*(opening/2+8),240),(28,28,480),'metal',False,yaw)
        box(name+'_Lamp'+str(sign),at(-70,sign*(opening/2+15),220),(12,10,120),'amber',False,yaw)
    box(name+'_Lintel',at(0,0,(height+480)/2),(90,opening,height-480),'concrete',True,yaw)
    if door_id:
        d=spawn(native('OBDoor'),name+'_Gate',at(0,0,240),yaw,tags=('L2Gate',))
        d.set_editor_properties(dict(door_id=door_id,panel_size=v((65,650,480)),open_distance=510.))
    return at(-100,0,110)


def tunnel(name, start, end, width=1150, height=760):
    dx,dy=end[0]-start[0],end[1]-start[1]
    length=math.hypot(dx,dy)
    yaw=math.degrees(math.atan2(dy,dx))
    mx,my=(start[0]+end[0])/2,(start[1]+end[1])/2
    # Kit meshes provide visible architecture; concealed simple collision is
    # independent of vendor complex collisions and remains deterministic.
    box(name+'_Foundation',(mx,my,-45),(length+30,width,90),'concrete',True,yaw)
    for sign in [-1,1]:
        rad=math.radians(yaw)
        p=(mx-sign*math.sin(rad)*(width/2+35),my+sign*math.cos(rad)*(width/2+35),height/2)
        a=box(name+'_Boundary'+str(sign),p,(length,70,height),'dark',True,yaw,folder='Collision')
        a.set_actor_hidden_in_game(True)
    roof=box(name+'_CeilingCollision',(mx,my,height*.8+45),(length,width,70),'dark',True,yaw,folder='Collision')
    roof.set_actor_hidden_in_game(True)
    count=max(1,math.ceil(length/900))
    for i in range(count):
        t=(i+.5)/count
        p=(start[0]+dx*t,start[1]+dy*t,height*.3)
        # Floor intersects the circular kit below its center, leaving a broad
        # flat chord: 1.35 * sqrt(1-.6^2) = 1.08 times the gameplay width.
        dims=(length/count+12,width*1.35,height)
        ayaw=yaw
        if TUNNEL_AXIS=='Y':
            dims=(dims[1],dims[0],dims[2]); ayaw-=90
        fitted(name+'_Tunnel%02d'%i,KIT+'/Meshes/Structural/SM_Tunnel01a',p,dims,ayaw)
        # Tracks retain their original materials and are safely nonblocking.
        fitted(name+'_Rail%02d'%i,KIT+'/Meshes/Structural/SM_Tunnel01a_TrackA',(p[0],p[1],8),(length/count,230,15),yaw)
        if i%2==0:
            light(name+'_Worklight%02d'%i,(p[0],p[1],height*.8-120),radius=1550,intensity=26000)
            prop(name+'_Fixture%02d'%i,'SM_Light01a',(p[0],p[1],height*.8-50),(110,36,24),yaw)
    ANCHORS.append((name+'_start',(start[0],start[1],110)))
    ANCHORS.append((name+'_end',(end[0],end[1],110)))


def room(name, center, size, openings=('W','E'), height=850, color=(.38,.52,.68)):
    x,y=center; sx,sy=size
    box(name+'_Floor',(x,y,-45),(sx,sy,90))
    box(name+'_Roof',(x,y,height+40),(sx,sy,80))
    for face in ['W','E','S','N']:
        horizontal=face in ['N','S']
        span=sx if horizontal else sy
        wallp=(x,y+(-sy/2 if face=='S' else sy/2),height/2) if horizontal else (x+(-sx/2 if face=='W' else sx/2),y,height/2)
        if face in openings:
            gap=min(1150,span-80)
            opening_height=min(760,height-80)
            for sign in [-1,1]:
                n=(span-gap)/2
                offset=sign*(gap/2+n/2)
                p=(wallp[0]+(offset if horizontal else 0),wallp[1]+(0 if horizontal else offset),height/2)
                box(name+'_'+face+str(sign),p,(n,70,height) if horizontal else (70,n,height))
            p=(wallp[0],wallp[1],(height+opening_height)/2)
            box(name+'_'+face+'_Head',p,(gap,70,height-opening_height) if horizontal else (70,gap,height-opening_height))
        else:
            box(name+'_'+face,wallp,(span,70,height) if horizontal else (70,span,height))
    # Installed concrete ribs, pipes and wear supply the room's visual language.
    for ix in [-1,1]:
        for iy in [-1,1]:
            prop(name+'_Column%d%d'%(ix,iy),'Sm_Beam03a',(x+ix*(sx/2-90),y+iy*(sy/2-90),height/2),(70,70,height))
            light(name+'_Lamp%d%d'%(ix,iy),(x+ix*sx*.27,y+iy*sy*.27,height-160),color,1600,26000)
    for sign in [-1,1]:
        prop(name+'_Pipe'+str(sign),'SM_Pipe04a_500cm',(x,y+sign*(sy/2-100),height-140),(sx-130,70,70))
    ANCHORS.append((name,(x,y,110)))


def console(name, action, zone, p, hold=2., yaw=180, tags=()):
    a=spawn(native('OBLevel2Interactable'),name,p,yaw,'Objectives',tags)
    a.set_editor_properties(dict(action=getattr(u.OBLevel2Action,action),stable_id=u.Name(name),zone=zone,hold_seconds=hold))
    INTERACTIONS.append(dict(id=name,action=action,zone=zone,position=list(p),hold_seconds=hold))
    return a


def zone(n,p,extent=(180,650,300),yaw=0):
    a=spawn(native('OBZoneTrigger'),'T%02d_Entry'%n,p,yaw,'Gameplay')
    a.set_editor_properties(dict(zone=n,trigger_extent=v(extent),checkpoint=False))
    return a


def machine(kind,eid,n,p,yaw=180):
    name='L2_E%03d_%02d'%(eid,n)
    a=spawn(native('OBMachineEnemy'),name,p,yaw,'Enemies')
    a.set_editor_properties(dict(kind=getattr(u.OBMachineKind,kind),encounter_id=eid,stable_id=u.Name(name)))
    MACHINES.append(dict(id=name,kind=kind,encounter=eid,position=list(p)))
    return a


def hazard(name,kind,p,extent,offset=0):
    a=spawn(native('OBLevel2Hazard'),name,p,folder='Hazards')
    a.set_editor_properties(dict(hazard_kind=getattr(u.OBLevel2HazardKind,kind),extent=v(extent),cycle_offset=offset))
    return a


def cover(name,p,size=(200,300,110),yaw=0):
    # Native collision shape keeps decorative crates from producing poor nav.
    box(name+'_Collision',(p[0],p[1],size[2]/2),size,'dark',True,yaw,folder='Cover')
    return prop(name,'SM_WoodCrate02a',(p[0],p[1],size[2]/2+3),size,yaw,False,folder='Cover')


def build_layout():
    room('T01_CrashChamber',(-100,0),(2800,2600),('W','E'),1050,(.5,.28,.16))
    tunnel('T01_FreightPortal',(-3000,0),(-1500,0),1700,1100)
    tunnel('T02_MaintenanceSpine',(1300,0),(4400,0),1150,760)
    room('T02_ServiceBend',(5000,0),(1200,1600),('W','N'),850)
    tunnel('T02_ServicePassage',(5000,800),(5000,2400),1150,760)
    room('T03_PumpJunction',(6500,3000),(4200,1600),('S','E'),1000,(.28,.48,.6))
    # The south opening is centered at x6500; connect the west bend to its
    # interior on a full floor under the boundary, with a dedicated west gap.
    box('T03_WestApproachFoundation',(5000,2400,-45),(1150,1200,90))
    # Explicit south access at x5000: split south wall generated above locally.
    for a in list(ACTORS.get_all_level_actors()):
        if u.Name(TAG) in a.tags and a.get_actor_label() in ['T03_PumpJunction_S-1','T03_PumpJunction_S1','T03_PumpJunction_S_Head']:
            ACTORS.destroy_actor(a)
    box('T03_SouthWallLeft',(4480,2200,500),(160,70,1000))
    box('T03_SouthWallRight',(7100,2200,500),(3000,70,1000))
    box('T03_SouthAccessHeader',(5000,2200,880),(1150,70,240))
    bulkhead('T03_DrainageLock',8500,3000,1600,1000,door_id=203)
    tunnel('T04_FreightGallery',(8600,3000),(11600,3000),2400,1100)
    room('T04_TransferBend',(12200,3000),(1200,2400),('W','N'),1050)
    tunnel('T04_FreightRise',(12200,4200),(12200,6000),1150,850)
    room('T05_AuxiliaryPower',(13000,7000),(2800,2000),('S','E'),1100,(.55,.38,.19))
    # Shift south opening toward freight approach x12200.
    for a in list(ACTORS.get_all_level_actors()):
        if u.Name(TAG) in a.tags and a.get_actor_label() in ['T05_AuxiliaryPower_S-1','T05_AuxiliaryPower_S1','T05_AuxiliaryPower_S_Head']:
            ACTORS.destroy_actor(a)
    box('T05_SouthWallLeft',(11620,6000,550),(40,70,1100))
    box('T05_SouthWallRight',(13600,6000,550),(1600,70,1100))
    box('T05_SouthAccessHeader',(12200,6000,975),(1150,70,250))
    bulkhead('T04_FreightExit',12200,5500,1150,850,yaw=90,door_id=204)
    bulkhead('T05_PowerExit',14350,7000,2000,1100,door_id=206)
    tunnel('T06_PressureLine',(14400,7000),(16400,7000),1150,850)
    room('T06_WardenAntechamber',(17400,7000),(2000,2400),('W','N'),1100,(.28,.4,.55))
    tunnel('T06_AntennaApproach',(17400,8200),(17400,9500),1150,850)
    room('T07_EntryTurn',(18000,9500),(2400,1300),('S','E'),850)
    # Shift entry opening to x17400, preserving a dry 1000cm passage.
    for a in list(ACTORS.get_all_level_actors()):
        if u.Name(TAG) in a.tags and a.get_actor_label() in ['T07_EntryTurn_S-1','T07_EntryTurn_S1','T07_EntryTurn_S_Head']:
            ACTORS.destroy_actor(a)
    box('T07_EntrySouthLeft',(16820,8850,425),(40,70,850))
    box('T07_EntrySouthRight',(18500,8850,425),(1300,70,850))
    bulkhead('T06_WardenSeal',18500,9500,1300,850,door_id=207)
    room('T07_SignalChamber',(20500,9500),(3600,3600),('W','E','N','S'),1500,(.38,.56,.65))
    # Room west spans x18700, giving 500cm overlap with entry turn; no gap.
    for i,p,yaw in [(1,(20500,7300),90),(2,(22700,9500),0),(3,(20500,11700),-90)]:
        room('T07_EntryDoor%d'%i,(p[0],p[1]),(1000,1000),('N','S') if i!=2 else ('W','E'),700,(.75,.16,.04))
        spawn(u.TargetPoint,'SecurityEntry_%d'%i,(p[0],p[1],100),yaw,tags=('L2SpawnDoor%d'%i,))
        text('SecurityDoorSign%d'%i,'AUTOMATED SECURITY // %02d'%i,(p[0]-100*math.cos(math.radians(yaw)),p[1]-100*math.sin(math.radians(yaw)),420),yaw+180,20,(255,145,70))
    # Close the far ends of the spawn pockets. Opening faces point into arena.
    box('T07_Door1Back',(20500,6750,350),(1100,80,700))
    box('T07_Door2Back',(23250,9500,350),(80,1100,700))
    box('T07_Door3Back',(20500,12250,350),(1100,80,700))
    # Occluding baffles make machine door arrival visible without popping in.
    for i,(x,y) in enumerate([(20500,7800),(22200,9500),(20500,11200)]):
        box('SecurityBaffle%d'%i,(x,y,180),(420,100,360) if i!=1 else (100,420,360),'metal')
    zone(2,(1700,0,200)); zone(3,(5000,2150,200),(650,100,300))
    zone(4,(8850,3000,200),(160,1300,300)); zone(5,(12200,5900,200),(650,100,300))
    zone(6,(14750,7000,200)); zone(7,(18620,9500,200),(80,750,300))


def dress_and_objectives():
    # Crash chamber: damaged recognizable craft, blocked route, cache and log.
    ship=fitted('StarSparrow_Wreck','/Game/StarSparrow/Meshes/Examples/SM_StarSparrow01',(-850,-500,235),None,8,False,'Cinematics',('L2Wreck',),True)
    o,e=bounds(ship.static_mesh_component.static_mesh)
    wreck=crash_wreck_placement((o.x,o.y,o.z),(e.x,e.y,e.z))
    ship.set_actor_scale3d(v(wreck['scale']))
    ship.set_actor_rotation(u.Rotator(pitch=-6,yaw=8,roll=-12),False)
    ship.set_actor_location(v(wreck['position']),False,False)
    for i,p in enumerate([(-1360,-420,130),(-1450,160,160),(-1330,680,125)]):
        prop('CrashDebris%d'%i,'SM_DebrisPile02b',p,(460,500,240),i*37)
    box('CollapsedPortal',(-3150,0,480),(180,2600,980),'concrete')
    prop('DetachedShipPanel','SM_MetalPanel02a',(-350,-920,45),(430,240,65),30)
    light('CrashEngineFire',(wreck['center'][0]-330,wreck['center'][1]+20,wreck['center'][2]+125),(1.,.18,.025),1400,37000,True)
    console('L2_CrashSupplies','SUPPLY',1,(600,-650,55),0)
    console('L2_CrashMedical','MEDICAL',1,(820,-650,55),0)
    console('L2_CrashLog','LOG',1,(650,650,55),0)
    text('T01_CommsSign','EXTERNAL COMMS OFFLINE\nEMERGENCY TRANSMITTER: SECTOR 07',(1160,0,390),180,32,(255,188,90))
    text('T01_Title','AURELIA FREIGHT NETWORK\nDECOMMISSIONED // 2189',(-500,1240,410),-90,28)
    # T02 first machines and cover introduce distinct silhouettes and telegraphs.
    for i,(x,y) in enumerate([(2550,-280),(3650,330),(4800,1500)]):
        cover('T02_PipeCache%d'%i,(x,y,0),(160,230,120))
    machine('CUTTER',201,1,(2600,100,110)); machine('CUTTER',201,2,(3200,-150,110))
    machine('WATCHER',202,1,(5000,900,110),-90); machine('WATCHER',202,2,(5300,1850,110),-90)
    text('T02_SectorSign','03 // DRAINAGE JUNCTION',(5000,2070,420),-90,28,(230,200,140))
    # T03 dry path surrounds live shallow water; there is never a required hit.
    fitted('T03_Water',KIT+'/Meshes/Effects/SM_WaterPlane01a_dense',(6600,3000,8),(2100,600,5),tags=('L2FloodWater',))
    hazard('LiveDrainageWater','LIVE_WATER',(6600,3000,30),(1050,300,60))
    for sign in [-1,1]:
        box('T03_DryWalkway'+str(sign),(6600,3000+sign*560,17),(2700,260,34),'metal')
    prop('T03_BrokenPowerCable','SM_ElectricalWire01a_Straight_1000',(6300,3000,250),(900,24,28),15)
    arc=light('T03_ElectricalArc',(6350,2900,80),(.12,.48,1),600,8000)
    arc.set_editor_property('tags',[u.Name(TAG),u.Name('L2ElectricalWarning')])
    console('L2_CableBreaker','BREAKER',3,(5400,2470,55),2)
    console('L2_DrainPump','PUMP',3,(7840,3530,55),2)
    text('T03_LiveWarning','DANGER // LIVE CABLE\nUSE DRY WALKWAYS',(5860,2550,250),180,24,(255,181,74))
    for i,(kind,x,y) in enumerate([('WATCHER',7580,3450),('CUTTER',6500,3490),('CUTTER',7200,2460)]):
        machine(kind,203,i+1,(x,y,110))
    # Freight gallery: open twin lanes, a side catwalk, authored cover and carts.
    for i,(x,y,s) in enumerate([(9500,2550,(250,440,145)),(10100,3420,(240,440,130)),(10800,2550,(220,350,150)),(11100,3500,(240,350,120))]):
        cover('T04_FreightCover%d'%i,(x,y,0),s)
    prop('T04_DisusedCrane','SM_Crane01a',(10600,3980,530),(900,300,900))
    box('T04_CatwalkDeck',(10200,3950,130),(2300,320,45),'metal')
    # Walkable broad ramps support capsules without requiring jumps.
    ramp=box('T04_CatwalkRamp',(8925,3950,65),(650,320,35),'metal')
    ramp.set_actor_rotation(u.Rotator(pitch=12,yaw=0,roll=0),False)
    ramp=box('T04_CatwalkExitRamp',(11600,3950,65),(650,320,35),'metal')
    ramp.set_actor_rotation(u.Rotator(pitch=-12,yaw=0,roll=0),False)
    for i,x in enumerate([9300,10300,11300]):
        prop('T04_CatwalkRailing%d'%i,'SM_Railing01a_Mid_A',(x,4160,190),(960,30,100))
    console('L2_FreightAlcoveSupplies','SUPPLY',4,(10950,4000,170),0)
    console('L2_FreightMedical','MEDICAL',4,(9400,2010,55),0)
    for i,(kind,x,y) in enumerate([('WATCHER',9500,3350),('WATCHER',10400,2440),('WATCHER',11100,3100),('CUTTER',9650,2750),('CUTTER',10750,3650)]):
        machine(kind,204,i+1,(x,y,110))
    machine('BULWARK',205,1,(12500,3950,110))
    text('T04_BulwarkMaintenanceSign','AUTOMATED MAINTENANCE // BAY04',(11540,3850,430),180,24,(255,190,95))
    text('T04_AuxDirection','AUXILIARY POWER →',(11780,3000,500),180,32,(255,204,120))
    # Power station: all critical components visibly close to one another.
    console('L2_CouplerCabinet','COUPLER',5,(12000,7600,55),0)
    console('L2_CouplerSocket','SOCKET',5,(13500,7560,55),1)
    console('L2_AuxRestart','POWER',5,(13900,7560,55),3)
    console('L2_PowerSupplies','SUPPLY',5,(13900,6430,55),0)
    for i,x in enumerate([12600,13200]):
        prop('T05_PowerCabinet%d'%i,'SM_ElectricalBox05c',(x,7850,280),(350,250,560))
    for i,(x,y) in enumerate([(12450,6550),(13100,7380),(13750,6720)]):
        machine('CUTTER' if i==0 else 'WATCHER',206,i+1,(x,y,110))
    light('T05_RestoredWorklights',(13000,7000,850),(.65,.72,.6),2300,45000,True,True)
    fan=prop('T05_PowerFan','SM_MetalCap03a',(13000,7950,630),(480,65,480),tags=('L2PowerFan',),movable=True)
    power_label=text('T05_PowerLabel','AUXILIARY NETWORK\nCOUPLER REQUIRED',(13600,7900,670),-90,30,(255,178,75))
    power_label.set_editor_property('tags',list(power_label.tags)+[u.Name('L2PowerLabel')])
    # Pressure route: 1150cm wide with safe waiting pockets by each valve.
    for i,x in enumerate([15000,15700,16200]):
        hazard('PressureVent_%d'%i,'PRESSURE_VENT',(x,7000,60),(120,270,100),i*1.5)
        prop('VentPipe_%d'%i,'SM_Pipe03a_joint_tjunc',(x,6570,150),(160,140,240))
        text('VentWarn_%d'%i,'PRESSURE RELEASE',(x-180,6600,260),180,18,(255,191,85))
    machine('WARDEN',207,1,(17700,7250,125))
    cover('T06_WardenCoverA',(16900,6250,0),(180,350,140))
    cover('T06_WardenCoverB',(17800,7780,0),(180,350,140))
    console('L2_WardenMedical','MEDICAL',6,(16650,6400,55),0)
    text('T06_WardenDesignation','RELAY WARDEN\nTRANSMITTER ACCESS RESTRICTED',(18100,7000,510),180,31,(255,135,75))
    # Signal chamber: central generator and cable shaft; circular movement.
    gen=fitted('EmergencySignalGenerator','/Engine/BasicShapes/Cylinder',(20500,9500,230),(420,420,460),0,True,'Objectives',('L2Generator',))
    gen.static_mesh_component.set_material(0,MATS['metal'])
    for z in [90,250,400]:
        ring=fitted('GeneratorRing_%d'%z,'/Engine/BasicShapes/Cylinder',(20500,9500,z),(460,460,30),0,False,'Objectives')
        ring.static_mesh_component.set_material(0,MATS['teal'])
    fitted('T07_AntennaCableShaft',KIT+'/Meshes/Structural/SM_ElevatorShaft01a_mid',(20500,9500,1190),(600,600,1350),0,False,'Architecture')
    prop('T07_AscendingCable','SM_ElectricalWire01a_Straight_1000',(20500,9500,1550),(35,35,1800))
    console('L2_StabilizerA','STABILIZER_A',7,(19900,8700,55),1.5,tags=('L2StabilizerA',))
    console('L2_StabilizerB','STABILIZER_B',7,(19900,10300,55),1.5,tags=('L2StabilizerB',))
    # Console platform has broad gentle steps, not mandatory precision movement.
    box('T07_ConsolePlatform',(21300,9500,35),(650,900,70),'metal')
    for i in range(3):
        box('T07_ConsoleStep%d'%i,(20850+i*80,9500,8+i*11),(100,700,16+i*22),'metal')
    console('L2_Transmitter','TRANSMITTER',7,(21300,9500,110),2)
    console('L2_FinaleAmmo','SUPPLY',7,(21500,10500,55),0)
    console('L2_FinaleMedical','MEDICAL',7,(21500,8500,55),0)
    for i,(x,y) in enumerate([(19600,9450),(20750,8350),(21500,10150),(20800,10600)]):
        cover('T07_DefenseCover%d'%i,(x,y,0),(220,360,145),i*90)
    for i,(x,y) in enumerate([(19900,8700),(19900,10300)]):
        text('StabilizerLabel%d'%i,'SIGNAL STABILIZER '+('A' if i==0 else 'B'),(x,y,245),180,23)
    text('T07_AntennaLabel','SURFACE ANTENNA // CABLE SHAFT',(20400,9300,640),180,28)
    light('T07_GeneratorCarrier',(20500,9500,630),(.08,.6,.42),2500,38000,True,True)
    # 13 exact finite finale machines. Positions remain inside announced doors.
    roster={210:['WATCHER','CUTTER','WATCHER','CUTTER'],211:['WATCHER','CUTTER','WATCHER','CUTTER'],212:['BULWARK','CUTTER','CUTTER'],213:['WATCHER','WATCHER']}
    pockets=[(20300,7240),(22750,9300),(20650,11720),(20560,7360)]
    for eid,kinds in roster.items():
        for i,kind in enumerate(kinds):
            px,py=pockets[(i+eid)%len(pockets)]
            machine(kind,eid,i+1,(px,py,125),math.degrees(math.atan2(9500-py,20500-px)))
    # Additional legible utility fixtures; no vendor blueprints or demo tracks.
    for i,(x,y,z) in enumerate([(600,1000,420),(4000,-530,350),(7800,3690,450),(9800,4120,560),(13200,7950,350),(17400,8100,410),(22000,11180,400)]):
        prop('UtilityBox_%d'%i,'SM_ElectricalBox05a',(x,y,z),(110,70,160))
    return ship


def cinematic_scenery():
    planet=fitted('Aurelia_OrbitalView','/Engine/BasicShapes/Sphere',(5000,-23000,10000),(20000,20000,20000),0,False,'Cinematics')
    planet.static_mesh_component.set_material(0,asset('/Game/OrbitBreakout/Materials/EnvironmentV3/M_Aurelia_V3'))
    # Reprise of the prison ship's orbital silhouette, behind the departing craft.
    for i in range(16):
        angle=i*math.pi/8
        p=(-16000+math.cos(angle)*2200,-18600,16000+math.sin(angle)*2200)
        a=box('PrisonOrbitalRing_%02d'%i,p,(900,180,130),'metal',False,folder='Cinematics')
        a.set_actor_rotation(u.Rotator(pitch=90+i*22.5,yaw=0,roll=0),False)
    for i in [-1,1]:
        box('PrisonSolarArray'+str(i),(-16000+i*3000,-18600,16000),(2000,2000,80),'dark',False,folder='Cinematics')
    # Deterministic sparse stars. These are distant set pieces, not lights.
    for i in range(42):
        x=-24000+(i*2297%37000); z=6000+(i*1777%19000)
        star=fitted('OrbitalStar_%02d'%i,'/Engine/BasicShapes/Sphere',(x,-35000,z),(18+i%4*6,)*3,0,False,'Cinematics')
        star.static_mesh_component.set_material(0,MATS['white'])
    light('OrbitalSun',(-11500,-17200,18000),(.85,.9,1),15000,1800000,True)
    light('OrbitalRim',(-10000,-20800,15500),(.1,.3,1),9000,800000)
    # Freight portal in a collapsed concrete/rock cutting. No physics simulation
    # is used for the essential flight path, scrape, or final wreck pose.
    box('ExteriorFreightApron',(-5300,0,-100),(6000,7000,200),'concrete')
    for i in range(12):
        side=-1 if i%2 else 1
        prop('PortalSlope_%02d'%i,'SM_DebrisPile02b',(-3600+(i%6)*230,side*(1300+(i%3)*350),200+(i%4)*180),(1300,1400,1200),i*23,False,folder='Cinematics')
    bulkhead('AureliaFreightPortal',-3100,0,4500,2400)
    text('PortalDesignation','AURELIA // FREIGHT ACCESS 07',(-3160,0,1600),180,90,(200,185,150))
    light('PortalEmergencyBeacon',(-3350,-1100,1000),(1,.17,.02),3500,150000,True)
    light('ExteriorDescentFill',(-8000,-2300,6500),(.48,.65,.85),12000,3000000,True)
    light('ExteriorPortalFill',(-4700,-200,2100),(.72,.65,.52),5000,1000000)
    # Leave a real hole in the chamber ceiling for the ascending antenna shaft.
    for a in list(ACTORS.get_all_level_actors()):
        if u.Name(TAG) in a.tags and a.get_actor_label()=='T07_SignalChamber_Roof':
            ACTORS.destroy_actor(a)
    for sign in [-1,1]:
        box('T07_RoofNorthSouth'+str(sign),(20500,9500+sign*1062.5,1540),(3600,1475,80))
        box('T07_RoofEastWest'+str(sign),(20500+sign*1062.5,9500,1540),(1475,650,80))
        box('SurfaceGroundNorthSouth'+str(sign),(20500,9500+sign*1262.5,2380),(4400,1875,160))
        box('SurfaceGroundEastWest'+str(sign),(20500+sign*1262.5,9500,2380),(1875,650,160))
    for i in range(3):
        fitted('AntennaShaft_%d'%i,KIT+'/Meshes/Structural/SM_ElevatorShaft01a_mid',(20500,9500,1550+i*400),(620,620,430),0,False,'Cinematics')
    pole=fitted('SurfaceAntennaMast','/Engine/BasicShapes/Cylinder',(20500,9500,3250),(70,70,1600),0,False,'Cinematics')
    pole.static_mesh_component.set_material(0,MATS['metal'])
    for i,z in enumerate([3350,3650,3900]):
        box('AntennaCrossbar%d'%i,(20500,9500,z),(800-i*160,70,50),'metal',False,folder='Cinematics')
    light('SurfaceAntennaCarrier',(20500,9500,3920),(.03,.7,.3),1100,16000)
    light('SurfaceDawn',(19300,8700,4500),(.35,.55,.8),5500,2000000,True)


def clean_sequence(name,duration):
    path=BASE+'/Cinematics/'+name
    seq=u.load_asset(path)
    if seq:
        for b in list(seq.get_bindings()):
            b.remove()
        for track in list(seq.get_tracks()):
            seq.remove_track(track)
    else:
        seq=ASSETS.create_asset(name,BASE+'/Cinematics',u.LevelSequence,u.LevelSequenceFactoryNew())
    if not seq:
        raise RuntimeError('Could not create editable sequence '+name)
    seq.set_display_rate(u.FrameRate(30,1))
    seq.set_playback_start(0)
    seq.set_playback_end(int(duration*30))
    return seq


def transform_track(seq,actor,keys,duration,keep=False):
    binding=seq.add_possessable(actor)
    binding.set_display_name(actor.get_actor_label())
    section=binding.add_track(u.MovieScene3DTransformTrack).add_section()
    section.set_range(0,int(duration*30))
    section.set_completion_mode(u.MovieSceneCompletionMode.KEEP_STATE if keep else u.MovieSceneCompletionMode.RESTORE_STATE)
    channels=section.get_channels_by_type(u.MovieSceneScriptingDoubleChannel)
    if len(channels)<9:
        raise RuntimeError('Expected nine transform channels for '+actor.get_actor_label())
    for seconds,pos,rot,scale in keys:
        values=tuple(pos)+tuple(rot)+tuple(scale) # rotations are Roll, Pitch, Yaw
        for ch,val in zip(channels,values):
            ch.add_key(u.FrameNumber(int(round(seconds*30))),float(val),0.,u.MovieSceneTimeUnit.DISPLAY_RATE,u.MovieSceneKeyInterpolation.LINEAR)
    return binding


def shot(seq,name,start,end,cam_from,cam_to,look_from,look_to,fov=65,tags=()):
    a=spawn(u.CameraActor,name,cam_from,folder='Cinematics',tags=tags)
    a.get_component_by_class(u.CameraComponent).set_field_of_view(fov)
    keys=[]
    for sec,pos,target in [(start,cam_from,look_from),(end,cam_to,look_to)]:
        r=u.MathLibrary.find_look_at_rotation(v(pos),v(target))
        keys.append((sec,pos,(r.roll,r.pitch,r.yaw),(1,1,1)))
    binding=transform_track(seq,a,keys,seq.get_playback_end()/30)
    a.set_actor_rotation(u.MathLibrary.find_look_at_rotation(v(cam_from),v(look_from)),False)
    track=next((t for t in seq.get_tracks() if isinstance(t,u.MovieSceneCameraCutTrack)),None)
    if not track:
        track=seq.add_track(u.MovieSceneCameraCutTrack)
    cut=track.add_section(); cut.set_range(int(start*30),int(end*30))
    cut.set_camera_binding_id(seq.get_binding_id(binding))
    return a


def crash_wreck_placement(origin,extent):
    # Preserve the spacecraft's authored proportions. The tilted bounds rest
    # just above the chamber floor while leaving the player's shoulder view clear.
    p,y,r=map(math.radians,(-6.,8.,-12.))
    cp,sp,cy,sy,cr,sr=math.cos(p),math.sin(p),math.cos(y),math.sin(y),math.cos(r),math.sin(r)
    axes=((cp*cy,cp*sy,sp),(sr*sp*cy-cr*sy,sr*sp*sy+cr*cy,-sr*cp),(-(cr*sp*cy+sr*sy),cy*sr-cr*sp*sy,cr*cp))
    scale=(.85,.85,.85)
    offset=tuple(sum(axes[j][i]*origin[j]*scale[j] for j in range(3)) for i in range(3))
    half=tuple(sum(abs(axes[j][i])*extent[j]*scale[j] for j in range(3)) for i in range(3))
    center=(-850.,-500.,half[2]+8.)
    return dict(position=tuple(center[i]-offset[i] for i in range(3)),center=center,rotation=(-12.,-6.,8.),scale=scale,world_extent=half)


def smoothstep(t):
    t=max(0.,min(1.,t))
    return t*t*(3.-2.*t)


def hermite(a,b,va,vb,t,duration):
    # Explicit per-frame samples avoid cubic-channel overshoot into architecture.
    h00=2*t*t*t-3*t*t+1; h10=t*t*t-2*t*t+t
    h01=-2*t*t*t+3*t*t; h11=t*t*t-t*t
    return tuple(h00*x+h10*duration*vx+h01*y+h11*duration*vy for x,y,vx,vy in zip(a,b,va,vb))


def crash_motion_keys(final,rotation,scale):
    keys=[(0,(-14300,-18000,16000),(0,0,0),scale),
          (5,(-11500,-18000,15700),(-16,-5,0),scale),
          (329/30.,(-8200,-18000,14300),(20,-16,5),scale)]
    # The sole spatial cut is aligned with the exterior camera cut at frame330.
    knots=[(11.,(-10500,0,4200),(570,0,-350),(8,-20,0)),
           (18.,(-6500,0,1750),(650,0,-330),(-13,-18,0)),
           (22.,(-3400,-120,520),(1000,-75,-130),(-20,-8,0)),
           (24.6,(final[0]-700,final[1]+100,final[2]+25),(500,-65,-25),(-14,-5,rotation[2])),
           (27.2,final,(0,0,0),rotation)]
    for frame in range(330,817):
        seconds=frame/30.
        segment=next((i for i in range(len(knots)-1) if seconds<=knots[i+1][0]),len(knots)-2)
        ta,a,va,ra=knots[segment];tb,b,vb,rb=knots[segment+1]
        t=max(0.,min(1.,(seconds-ta)/(tb-ta)))
        pos=hermite(a,b,va,vb,t,tb-ta)
        ease=smoothstep(t)
        rot=tuple(x+(y-x)*ease for x,y in zip(ra,rb))
        keys.append((seconds,pos,rot,scale))
    keys.append((42.,final,rotation,scale))
    return keys


def keyed_crash_shot(seq,name,start,end,keys,fov):
    a=spawn(u.CameraActor,name,keys[0][1],folder='Cinematics')
    a.camera_component.set_field_of_view(fov)
    a.set_actor_rotation(u.Rotator(roll=keys[0][2][0],pitch=keys[0][2][1],yaw=keys[0][2][2]),False)
    binding=transform_track(seq,a,keys,42)
    track=next((t for t in seq.get_tracks() if isinstance(t,u.MovieSceneCameraCutTrack)),None)
    if not track:track=seq.add_track(u.MovieSceneCameraCutTrack)
    cut=track.add_section();cut.set_range(int(start*30),int(end*30));cut.set_camera_binding_id(seq.get_binding_id(binding))
    return a


def crash_sequence(ship):
    crash=clean_sequence('LS_DeadSignal_Crash',42)
    loc=ship.get_actor_location();rot=ship.get_actor_rotation();scale=ship.get_actor_scale3d()
    final=(loc.x,loc.y,loc.z);sc=(scale.x,scale.y,scale.z);rotation=(rot.roll,rot.pitch,rot.yaw)
    shipkeys=crash_motion_keys(final,rotation,sc)
    transform_track(crash,ship,shipkeys,42,True)
    shot(crash,'CrashCam01_Escape',0,5,(-15300,-16300,16900),(-12500,-16300,16400),(-14200,-18000,16000),(-11400,-18000,15700),72)
    shot(crash,'CrashCam02_Failure',5,11,(-10600,-17200,15900),(-7600,-17300,14800),(-11500,-18000,15700),(-8200,-18000,14300),66)
    shot(crash,'CrashCam03_Descent',11,18,(-9200,-2600,3600),(-7100,-2000,2150),(-10500,0,4200),(-6500,0,1750),75)
    shot(crash,'CrashCam04_Impact',18,24,(-7900,-2400,2500),(-5000,-1600,1400),(-6500,0,1750),(-3000,-130,450),78)
    # The impact is framed from clear floor north/east of the wreck. There is no
    # long camera travel past the hull while the craft is coming to rest.
    keys=[]
    for frame in range(720,1021):
        seconds=frame/30.;ease=smoothstep((seconds-24)/10.)
        pos=tuple(a+(b-a)*ease for a,b in zip((650,920,450),(350,720,360)))
        target=tuple(a+(b-a)*ease for a,b in zip((-1850,-350,290),(-300,-150,180)))
        r=u.MathLibrary.find_look_at_rotation(v(pos),v(target))
        keys.append((seconds,pos,(r.roll,r.pitch,r.yaw),(1,1,1)))
    keyed_crash_shot(crash,'CrashCam05_Wreck',24,34,keys,76)
    # Match the actual grounded pawn's normal320cm arm,35/65 socket,−8° pitch
    # and85° FOV. The last1.5s are stationary before control returns.
    pitch=math.radians(-8.)
    end=(-320*math.cos(pitch)-65*math.sin(pitch),35.,98.-320*math.sin(pitch)+65*math.cos(pitch))
    initial=(-380,480,260)
    r=u.MathLibrary.find_look_at_rotation(v(initial),v((0,0,150)))
    keys=[]
    for frame in range(1020,1261):
        seconds=frame/30.;ease=smoothstep((seconds-34.)/6.5)
        pos=tuple(a+(b-a)*ease for a,b in zip(initial,end))
        rotation=(0.,r.pitch+(-8.-r.pitch)*ease,r.yaw*(1.-ease))
        keys.append((seconds,pos,rotation,(1,1,1)))
    keyed_crash_shot(crash,'CrashCam06_Recovery',34,42,keys,85)
    REPORT['wreck_final_transform']={'position':list(final),'scale':list(sc),'rotation':[rot.roll,rot.pitch,rot.yaw]}
    return crash


def sequences(ship):
    crash=crash_sequence(ship)
    rescue=clean_sequence('LS_DeadSignal_Rescue',15)
    shot(rescue,'RescueCam01_Call',0,6,(21800, 10050, 390),(21700, 9920, 360),(21170, 9500, 205),(21170, 9500, 205),62,('L2RescueCamera',))
    shot(rescue,'RescueCam02_Carrier',6,11,(21680, 8850, 300),(21580, 8940, 280),(21160, 9500, 225),(21160, 9500, 225),58,())
    shot(rescue,'RescueCam03_Response',11,15,(21590, 9770, 260),(21510, 9740, 245),(21135, 9500, 230),(21135, 9500, 230),54,())
    for seq in [crash,rescue]:
        if not u.EditorAssetLibrary.save_loaded_asset(seq,False):
            raise RuntimeError('Could not save sequence '+seq.get_name())
    REPORT['sequences']=[dict(path=crash.get_path_name(),duration_seconds=42,camera_cuts=6),dict(path=rescue.get_path_name(),duration_seconds=15,camera_cuts=3)]
    return crash,rescue


def navigation_world():
    world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
    ws=world.get_world_settings()
    ws.set_editor_properties(dict(default_game_mode=native('OBGameMode'),kill_z=-1200.,enable_world_bounds_checks=True))
    nav=spawn(u.NavMeshBoundsVolume,'Nav_DeadSignal',(11000,5000,600),folder='Navigation')
    _,e=nav.get_actor_bounds(False)
    if min(e.x,e.y,e.z)<=0:
        raise RuntimeError('Editor volume actor factory created no navigation brush')
    nav.set_actor_scale3d(v((13000/e.x,7800/e.y,1100/e.z)))
    ns=u.NavigationSystemV1.get_navigation_system(world)
    if ns:
        ns.on_navigation_bounds_updated(nav)
    post=spawn(u.PostProcessVolume,'DeadSignalExposure',(0,0,0),folder='Lighting')
    post.set_editor_property('unbound',True)
    settings=post.get_editor_property('settings')
    for key,val in [('override_auto_exposure_min_brightness',True),('override_auto_exposure_max_brightness',True),('auto_exposure_min_brightness',5.5),('auto_exposure_max_brightness',5.5),('override_motion_blur_amount',True),('motion_blur_amount',0.),('override_bloom_intensity',True),('bloom_intensity',.2),('override_ambient_occlusion_intensity',True),('ambient_occlusion_intensity',1.)]:
        settings.set_editor_property(key,val)
    post.set_editor_property('settings',settings)
    return world


def backup_generated():
    stamp=datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ')
    target=ROOT/'Saved'/'Backups'/('DeadSignal_'+stamp)
    files=[ROOT/'Content'/'OrbitBreakout'/'Maps'/'L_DeadSignal.umap']
    cine=ROOT/'Content'/'OrbitBreakout'/'Level2'/'Cinematics'
    if cine.exists():
        files.extend(cine.glob('*.uasset'))
    copied=[]
    for src in files:
        if src.is_file():
            dst=target/src.relative_to(ROOT); dst.parent.mkdir(parents=True,exist_ok=True)
            shutil.copy2(src,dst); copied.append(src.relative_to(ROOT).as_posix())
    if copied:
        (target/'MANIFEST.json').write_text(json.dumps({'files':copied,'purpose':'Pre-regeneration backup; only chapter-two generated assets'},indent=2))
        REPORT['backup']=str(target)


def project(world,p):
    result=u.NavigationSystemV1.project_point_to_navigation(world,v(p),None,None,v((110,110,260)))
    if hasattr(result,'x'):
        return result
    if isinstance(result,tuple):
        return next((r for r in result if hasattr(r,'x')),None) if not any(r is False for r in result) else None
    return None


def validate_nav(world):
    projections=[]; paths=[]; errors=[]
    # Gameplay points only. Cinematic orbit/surface sets have no navigation.
    points=[('CrashCheckpoint',(0,0,110)),('JunctionCheckpoint',(6500,3500,110)),('PowerCheckpoint',(13000,7000,110)),('FinaleCheckpoint',(18900,9500,110))]
    points += [(m['id'],m['position']) for m in MACHINES]
    for name,p in points:
        q=project(world,p)
        projections.append(dict(id=name,valid=q is not None,location=[q.x,q.y,q.z] if q is not None else None))
        if q is None: errors.append('No nav projection: '+name)
    # Each segment stays on its own side of a closed mission gate. Dynamic
    # Recast updates those gates when native gameplay opens them at runtime.
    checks=[('MaintenanceBend',(1600,0,110),(5000,1800,110)),('PumpDryBypass',(5000,2500,110),(8100,3500,110)),('FreightFlanks',(9000,3000,110),(11800,3500,110)),('AuxiliaryRoom',(12200,6300,110),(13900,7000,110)),('PressureLine',(14700,7000,110),(18000,7600,110)),('WardenToFinaleSeal',(17400,7900,110),(18200,9500,110)),('FinaleCircle',(19200,9500,110),(21600,10400,110)),('SpawnDoorSouth',(20300,7240,110),(20400,8500,110)),('SpawnDoorEast',(22750,9300,110),(21500,9500,110)),('SpawnDoorNorth',(20650,11720,110),(20700,10200,110))]
    for name,a,b in checks:
        pa,pb=project(world,a),project(world,b)
        path=u.NavigationSystemV1.find_path_to_location_synchronously(world,pa,pb,None,None) if pa is not None and pb is not None else None
        valid=bool(path and path.is_valid() and not path.is_partial())
        paths.append(dict(id=name,valid=valid,length_cm=path.get_path_length() if valid else None))
        if not valid: errors.append('No complete nav path: '+name)
    return dict(projections=projections,paths=paths,errors=errors,status='PASS' if not errors else 'FAIL',scope='Editor navmesh projections and ten connected local routes; gameplay traversal and closed-gate opening require runtime tests')


def write_report():
    out=ROOT/'Saved'/'Level2';out.mkdir(parents=True,exist_ok=True)
    (out/'BUILD_RESULT.json').write_text(json.dumps(REPORT,indent=2)+'\n')


def wait_navigation(world):
    global _NAV
    state={'started':time.monotonic(),'rebuilt':False,'idle':None,'keep':u.EditorPythonScripting.get_keep_python_script_alive()}
    _NAV=state
    u.EditorPythonScripting.set_keep_python_script_alive(True)
    def tick(dt):
        global _NAV
        try:
            elapsed=time.monotonic()-state['started']
            if elapsed>180:
                raise RuntimeError('Navigation did not settle within 180 seconds')
            if u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()!=world:
                raise RuntimeError('Editor world changed during generation validation')
            if elapsed<2 or u.NavigationSystemV1.is_navigation_being_built_or_locked(world):
                state['idle']=None; return True
            if not state['rebuilt']:
                u.SystemLibrary.execute_console_command(world,'RebuildNavigation')
                state['rebuilt']=True;state['idle']=None;return True
            if state['idle'] is None:
                state['idle']=time.monotonic();return True
            if time.monotonic()-state['idle']<3:
                return True
            REPORT['navigation']=validate_nav(world)
            if not LEVELS.save_current_level():
                raise RuntimeError('Navigation-complete map save failed')
            REPORT['saved_after_navigation']=True
            REPORT['status']='PASS_STRUCTURE_AND_NAVIGATION' if not REPORT['navigation']['errors'] else 'SAVED_NAVIGATION_FAILED'
            REPORT['finished_utc']=datetime.now(timezone.utc).isoformat()
            write_report()
            u.log('DEAD_SIGNAL_BUILD_COMPLETE '+REPORT['status'])
        except Exception as exc:
            REPORT['status']='FAILED';REPORT['error']=str(exc);write_report();u.log_error(traceback.format_exc())
        u.EditorPythonScripting.set_keep_python_script_alive(state['keep']);_NAV=None
        return False
    state['handle']=u.register_ticker_callback(tick,.25)


def build():
    if ROOT.name!='OrbitBreakout' or not (ROOT/'Source'/'OrbitBreakout'/'OBLevel2.h').exists():
        raise RuntimeError('Refusing to author outside the real OrbitBreakout project')
    if u.get_editor_subsystem(u.UnrealEditorSubsystem).get_game_world():
        raise RuntimeError('Stop PIE before generating Dead Signal')
    for name in ['OBLevel2Director','OBLevel2Interactable','OBLevel2Hazard','OBMachineEnemy','OBDoor','OBZoneTrigger','OBGameMode']:
        native(name)
    backup_generated()
    if u.EditorAssetLibrary.does_asset_exist(MAP):
        if not LEVELS.load_level(MAP):raise RuntimeError('Cannot load '+MAP)
        for a in list(ACTORS.get_all_level_actors()):
            if u.Name(TAG) in a.tags:ACTORS.destroy_actor(a)
    elif not LEVELS.new_level(MAP):raise RuntimeError('Cannot create '+MAP)
    materials()
    # Record measured mesh metadata; no assumptions about pivot are needed.
    REPORT['kit_mesh_bounds']={}
    for name in ['SM_Tunnel01a','SM_Tunnel01a_TrackA','SM_ElevatorShaft01a_mid']:
        o,e=bounds(asset(KIT+'/Meshes/Structural/'+name))
        REPORT['kit_mesh_bounds'][name]=dict(origin=[o.x,o.y,o.z],extent=[e.x,e.y,e.z])
    build_layout(); ship=dress_and_objectives(); cinematic_scenery()
    crash,rescue=sequences(ship)
    director=spawn(native('OBLevel2Director'),'Mission_DeadSignal',(0,0,0),folder='Gameplay')
    director.set_editor_properties(dict(start_location=v((0,0,110)),junction_checkpoint=v((6500,3500,110)),power_checkpoint=v((13000,7000,110)),finale_checkpoint=v((18900,9500,110)),crash_sequence=crash,rescue_sequence=rescue,crash_duration=42.,rescue_duration=15.))
    spawn(u.PlayerStart,'Rook_CrashStart',(0,0,110),0,'Gameplay')
    world=navigation_world()
    if not LEVELS.save_current_level():raise RuntimeError('Initial map save failed')
    if not u.EditorAssetLibrary.save_directory(BASE,only_if_is_dirty=True,recursive=True):raise RuntimeError('Chapter asset save failed')
    REPORT.update(actor_counts=COUNTS,actor_total=len(ACTORS.get_all_level_actors()),saved=True,scope='Real map and authored sequences. Runtime playability verification is separate.',status='SAVED_NAVIGATION_PENDING')
    manifest=dict(map=MAP,tag=TAG,checkpoints={'Crash':[0,0,110],'Junction':[6500,3500,110],'Power':[13000,7000,110],'Finale':[18900,9500,110]},interactions=INTERACTIONS,machines=MACHINES,anchors=ANCHORS,doors=[203,204,206,207],finale_roster_count=len([m for m in MACHINES if m['encounter']>=210]),sequence_paths=[s['path'] for s in REPORT['sequences']])
    docs=ROOT/'docs';docs.mkdir(exist_ok=True)
    (docs/'LEVEL_2_LAYOUT_MANIFEST.json').write_text(json.dumps(manifest,indent=2)+'\n')
    write_report()
    u.get_editor_subsystem(u.UnrealEditorSubsystem).set_level_viewport_camera_info(v((300,-1050,600)),u.Rotator(pitch=-15,yaw=85,roll=0))
    u.AutomationLibrary.finish_loading_before_screenshot()
    wait_navigation(world)


try:
    build()
except Exception as exc:
    REPORT['status']='FAILED';REPORT['error']=str(exc);write_report()
    u.log_error('DEAD_SIGNAL_BUILD_FAILED '+traceback.format_exc())
    raise
