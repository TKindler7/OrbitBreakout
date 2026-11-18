"""Read-only authored Wraith pose sampling; run in editor or Python commandlet."""
import json
from pathlib import Path
import unreal as u

root='/Game/ParagonWraith/Characters/Heroes/Wraith/Animations/'
result={}
for name in ['Level_Start','Jump_Recovery','Idle_NonCombat','Idle_Ability_RMB']:
    anim=u.load_asset(root+name)
    length=anim.get_play_length()
    row={'asset':anim.get_path_name(),'length':length,'samples':[]}
    for time in sorted(set([0.0,.1,.25,.5,.75,1.0,1.5,2.0,3.0,4.0,max(0,length-.01)])):
        if time>length:continue
        pose=u.AnimPoseExtensions.get_anim_pose_at_time(anim,time,u.AnimPoseEvaluationOptions())
        bones={}
        for name_bone in ['root','pelvis','foot_l','foot_r','head','hand_r']:
            t=u.AnimPoseExtensions.get_bone_pose(pose,name_bone,u.AnimPoseSpaces.WORLD)
            v=t.translation;q=t.rotation
            bones[name_bone]={'position':[v.x,v.y,v.z],'rotation':[q.x,q.y,q.z,q.w]}
        row['samples'].append({'seconds':time,'bones':bones})
    result[name]=row
out=Path(u.Paths.project_saved_dir())/'Level2'/'CRASH_RECOVERY_ASSET_POSES.json'
out.parent.mkdir(parents=True,exist_ok=True)
out.write_text(json.dumps(result,indent=2)+'\n')
u.log('OB_CRASH_RECOVERY_ASSET_POSES '+str(out))
