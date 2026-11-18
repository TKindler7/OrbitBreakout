"""Create original service-loader assemblies. Blender -b --python author_loader.py -- OUTPUT."""
from pathlib import Path
import sys
import bpy
from mathutils import Vector, Matrix

out = Path(sys.argv[sys.argv.index('--') + 1])
out.mkdir(parents=True, exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
palette = {'Ochre':(.65,.24,.035), 'Carbon':(.025,.035,.04), 'Gunmetal':(.09,.14,.15),
           'Alloy':(.42,.48,.51), 'Glazing':(.013,.068,.086), 'Ivory':(.8,.8,.65),
           'Cyan':(.04,.75,.82), 'Amber':(1,.26,.015)}
materials = {}
for name,color in palette.items():
    m=bpy.data.materials.new(name)
    m.diffuse_color=(*color,1)
    m.use_nodes=True
    m.node_tree.nodes.get('Principled BSDF').inputs['Base Color'].default_value=(*color,1)
    materials[name]=m

# Model in centimeters with X forward, Y starboard and Z up.
def xyz(p):
    return Vector((p[0]/100,-p[1]/100,p[2]/100))

def finish(obj,name,material,bevel=0):
    obj.name=name
    obj.data.materials.append(materials[material])
    if bevel:
        mod=obj.modifiers.new('Machined edges','BEVEL')
        mod.width=bevel/100
        mod.segments=2
        mod.harden_normals=True
        bpy.ops.object.modifier_apply(modifier=mod.name)
    parts.append(obj)

def box(name, pos, dims, material, bevel=1):
    bpy.ops.mesh.primitive_cube_add(size=1,location=xyz(pos))
    obj=bpy.context.object
    obj.dimensions=tuple(d/100 for d in dims)
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    finish(obj,name,material,bevel)

def rod(name, a, b, radius, material, tip=None, sides=12):
    a,b=xyz(a),xyz(b)
    bpy.ops.mesh.primitive_cone_add(vertices=sides,radius1=radius/100,radius2=(radius if tip is None else tip)/100,
                                   depth=(b-a).length,location=(a+b)/2)
    obj=bpy.context.object
    obj.rotation_euler=(b-a).to_track_quat('Z','Y').to_euler()
    finish(obj,name,material)

def shell(name, rings, material, bevel=1):
    verts=[]
    for x,y,z,w,h,c in rings:
        corners=[(-w+c,-h),(w-c,-h),(w,-h+c),(w,h-c),(w-c,h),(-w+c,h),(-w,h-c),(-w,-h+c)]
        verts.extend(xyz((x+a,y+b,z)) for a,b in corners)
    faces=[tuple(range(8)),tuple(reversed(range(len(verts)-8,len(verts))))]
    for j in range(len(rings)-1):
        for i in range(8):
            a=j*8+i;b=j*8+(i+1)%8
            faces.append((a,a+8,b+8,b))
    mesh=bpy.data.meshes.new(name);mesh.from_pydata(verts,[],faces);mesh.update()
    obj=bpy.data.objects.new(name,mesh);bpy.context.collection.objects.link(obj)
    bpy.context.view_layer.objects.active=obj
    finish(obj,name,material,bevel)

def bearing(x,z,r):
    rod('Axle',(x,-11,z),(x,11,z),r,'Carbon',sides=20)
    for s in (-1,1):
        rod('Bearing cap',(x,s*11,z),(x,s*14,z),r*.82,'Alloy',sides=16)
        rod('Axle lock',(x,s*14,z),(x,s*15,z),r*.4,'Gunmetal',sides=6)

def hull():
    shell('Hip casting',[(0,0,-13,21,42,8),(0,0,1,28,48,10),(-5,0,29,30,44,9)],'Carbon',2)
    shell('Cab tub',[(-6,0,18,27,38,8),(2,0,40,45,50,12),(4,0,57,48,48,10)],'Ochre',2)
    shell('Canopy seal',[(5,0,56,48,48,11),(5,0,61,48,48,11)],'Carbon')
    shell('Armored glazing',[(5,0,61,47,47,11),(-1,0,102,35,42,9),(-3,0,110,30,40,8)],'Glazing')
    shell('Cab roof',[(-3,0,106,33,43,8),(-4,0,117,36,45,10),(-7,0,125,30,38,9)],'Ochre',2)
    for s in (-1,1):
        rod('Canopy rail',(48,s*35,60),(28,s*34,110),3.2,'Alloy')
        rod('Mullion',(52,s*15,60),(32,s*14,108),1.3,'Gunmetal')
        box('Headlamp housing',(43,s*32,48),(13,19,9),'Carbon',2)
        box('Headlamp',(50,s*32,48),(2,14,3),'Cyan',.4)
        for i in range(5):box('Intake fin',(14-i*7,s*49,45),(3,3,12),'Carbon',.4)
        rod('Shoulder trunnion',(-5,s*42,57),(-5,s*64,57),14,'Gunmetal',sides=16)
        shell('Shoulder armor',[(-5,s*60,60,17,11,4),(-8,s*60,78,21,14,5),(-11,s*60,84,17,11,4)],'Ochre',1.5)
        box('Reflective band',(43,s*29,39),(4,15,2),'Ivory',.3)
    box('Rear exchanger',(-43,0,62),(27,63,54),'Gunmetal',3)
    for z in range(41,84,5):box('Radiator louvre',(-58,0,z),(6,50,2),'Carbon',.5)
    for s in (-1,1):
        rod('Exhaust',(-43,s*22,82),(-46,s*22,105),6,'Alloy')
        rod('Exhaust cap',(-46,s*22,103),(-46,s*22,107),6.8,'Carbon')
    rod('Pulse breech',(-8,61,53),(42,61,53),11,'Gunmetal')
    for x in (15,24,33,42):rod('Heat collar',(x,61,53),(x+4,61,53),12,'Alloy')
    rod('Emitter shroud',(43,61,53),(68,48,52),10,'Ochre',8)
    rod('Emitter rim',(65,49,52),(70,47,52),8.5,'Carbon')
    rod('Pulse coil',(70,47,52),(71,47,52),4.8,'Cyan')
    rod('Salvage boom',(-8,-60,55),(16,-61,19),8,'Gunmetal')
    rod('Hydraulic shaft',(-15,-68,58),(9,-69,23),3,'Alloy')
    box('Wrist',(18,-61,17),(18,17,15),'Ochre',2)
    for s in (-1,1):
        rod('Jaw knuckle',(18,-61+s*5,17),(36,-61+s*11,12),4,'Gunmetal')
        rod('Jaw tooth',(36,-61+s*11,12),(44,-61+s*6,9),3.5,'Alloy',1.4)
    rod('Beacon',(-13,24,125),(-13,24,132),3.5,'Amber')

def thigh():
    bearing(0,0,13)
    rod('Femur',(0,0,-3),(15,0,-48),11,'Gunmetal',8)
    shell('Femur plate',[(14,0,-44,9,10,3),(6,0,-23,14,14,5),(1,0,-10,12,13,4)],'Ochre')
    for s in (-1,1):rod('Upper ram',(-7,s*9,-5),(6,s*9,-43),2.6,'Alloy')
    bearing(15,-51,11)

def shin():
    rod('Lower spar',(0,0,0),(-20,0,-57),7.5,'Gunmetal',6)
    shell('Shin guard',[(-12,0,-42,7,9,2),(-3,0,-16,11,13,4),(0,0,-7,10,12,4)],'Ochre')
    rod('Ram sleeve',(3,0,-6),(-4,0,-33),4.2,'Carbon')
    for s in (-1,1):rod('Piston rod',(-2,s*9,-8),(-19,s*9,-52),2.2,'Alloy')
    bearing(-20,-56,8)

def foot():
    shell('Boot',[(4,0,-26,26,15,6),(7,0,-19,28,16,6),(1,0,-11,19,13,4),(-2,0,-4,11,9,3)],'Gunmetal')
    box('Ankle bracket',(0,0,-5),(17,19,13),'Ochre',2)
    for x in (-14,-3,8,19,30):box('Tread',(x,0,-28),(8,29,4),'Carbon',.6)

def cockpit():
    for s in (-1,1):
        rod('Windshield pillar',(42,s*31,-21),(47,s*36,24),2,'Gunmetal',1.6)
        rod('Gasket',(43,s*30,-20),(48,s*35,23),.7,'Carbon')
        rod('Grab rail',(30,s*26,-18),(35,s*31,-9),.9,'Alloy')
        box('Instrument housing',(41,s*27,-17),(15,14,7),'Gunmetal')
        box('Panel',(33.1,s*27,-16.5),(1,10,4),'Carbon',.35)
        for i in range(4):
            box('Diagnostic lamp',(32.5,s*(23+i*2),-16),(1,1,.7),'Amber' if i==3 else 'Cyan',.1)
    rod('Brow crossbar',(47,-37,24),(47,37,24),1.8,'Carbon')
    box('Crash rail',(45,0,-22),(10,64,4),'Carbon',1)
    for s in (-1,1):
        for i in range(3):box('Instrument tick',(39.5,s*(13+i*2),-21.4),(1.4,1,1.2),'Ivory',.1)

for name,builder in [('Hull',hull),('Thigh',thigh),('Shin',shin),('Foot',foot),('Cockpit',cockpit)]:
    parts=[]
    builder()
    bpy.ops.object.select_all(action='DESELECT')
    for obj in parts:obj.select_set(True)
    bpy.context.view_layer.objects.active=parts[0]
    bpy.ops.object.join()
    obj=bpy.context.object
    obj.data.transform(Matrix.Scale(100,4) @ obj.matrix_world)  # FBX geometry in Unreal centimeters
    obj.matrix_world=Matrix.Identity(4)
    obj.name='SM_Loader'+name
    bpy.context.view_layer.update()
    bpy.ops.export_scene.fbx(filepath=str(out/(obj.name+'.fbx')),use_selection=True,apply_unit_scale=True,
        apply_scale_options='FBX_SCALE_UNITS',object_types={'MESH'},bake_anim=False,axis_forward='-Y',axis_up='Z')
    print('LOADER_EXPORT',name,len(obj.data.polygons))
