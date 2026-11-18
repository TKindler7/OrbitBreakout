"""Import the five authored FBX assemblies from ArtSource/Loader/Geometry into Unreal."""
from pathlib import Path
import json
import unreal as u

root=Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir()))
dest='/Game/OrbitBreakout/Loader'
assets=u.EditorAssetLibrary
tools=u.AssetToolsHelpers.get_asset_tools()
edit=u.MaterialEditingLibrary
palette={'Ochre':(.65,.24,.035),'Carbon':(.025,.035,.04),'Gunmetal':(.09,.14,.15),
         'Alloy':(.42,.48,.51),'Glazing':(.013,.068,.086),'Ivory':(.8,.8,.65),
         'Cyan':(.04,.75,.82),'Amber':(1,.26,.015)}
slots={}
for name,color in palette.items():
    label='MI_Loader'+name
    mat=assets.load_asset(dest+'/'+label) if assets.does_asset_exist(dest+'/'+label) else None
    if not mat:
        mat=tools.create_asset(label,dest,u.MaterialInstanceConstant,u.MaterialInstanceConstantFactoryNew())
    glow=name in ('Cyan','Amber')
    parent='/Game/OrbitBreakout/Materials/'+('M_Emissive' if glow else 'M_Metal')
    edit.set_material_instance_parent(mat,assets.load_asset(parent))
    edit.set_material_instance_vector_parameter_value(mat,'Color',u.LinearColor(*color,1))
    edit.set_material_instance_scalar_parameter_value(mat,'Roughness',.19 if name=='Glazing' else .46)
    if glow:edit.set_material_instance_scalar_parameter_value(mat,'EmissiveStrength',1.8)
    assets.save_loaded_asset(mat)
    slots[name]=mat
report={}
for part in ('Hull','Cockpit','Thigh','Shin','Foot'):
    name='SM_Loader'+part
    task=u.AssetImportTask()
    task.filename=str(root/'ArtSource'/'Loader'/'Geometry'/(name+'.fbx'))
    task.destination_path=dest
    task.destination_name=name
    task.automated=True
    task.replace_existing=True
    task.save=False
    task.factory=u.FbxFactory()
    options=u.FbxImportUI()
    options.automated_import_should_detect_type=False
    options.mesh_type_to_import=u.FBXImportType.FBXIT_STATIC_MESH
    options.import_mesh=True
    options.import_materials=False
    options.import_textures=False
    options.static_mesh_import_data.combine_meshes=True
    options.static_mesh_import_data.auto_generate_collision=False
    task.options=options
    tools.import_asset_tasks([task])
    mesh=assets.load_asset(dest+'/'+name)
    if not mesh:raise RuntimeError(name)
    materials=mesh.static_materials
    for i,slot in enumerate(materials):
        key=str(slot.material_slot_name)
        mesh.set_material(i,slots[key])
    if not assets.save_loaded_asset(mesh,False):raise RuntimeError('Save failed for '+name)
    bounds=mesh.get_bounding_box()
    report[name]={'min':[bounds.min.x,bounds.min.y,bounds.min.z],'max':[bounds.max.x,bounds.max.y,bounds.max.z],
                  'slots':[str(s.material_slot_name) for s in mesh.static_materials]}
hull=report['SM_LoaderHull']
if not (125 < hull['max'][2] < 140 and 65 < hull['max'][0] < 80):
    raise RuntimeError('Loader scale or axis incorrect: '+str(hull))
(root/'loader-import.json').write_text(json.dumps(report,indent=2))
