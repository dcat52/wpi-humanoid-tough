## *********************************************************
## 
## File autogenerated for the val_task1 package 
## by the dynamic_reconfigure package.
## Please do not edit.
## 
## ********************************************************/

from dynamic_reconfigure.encoding import extract_params

inf = float('inf')

config_description = {'upper': 'DEFAULT', 'lower': 'groups', 'srcline': 233, 'name': 'Default', 'parent': 0, 'srcfile': '/opt/ros/indigo/lib/python2.7/dist-packages/dynamic_reconfigure/parameter_generator.py', 'cstate': 'true', 'parentname': 'Default', 'class': 'DEFAULT', 'field': 'default', 'state': True, 'parentclass': '', 'groups': [{'upper': 'PANELWALKPOSE', 'lower': 'panelwalkpose', 'srcline': 107, 'name': 'panelWalkPose', 'parent': 0, 'srcfile': '/opt/ros/indigo/lib/python2.7/dist-packages/dynamic_reconfigure/parameter_generator.py', 'cstate': 'true', 'parentname': 'Default', 'class': 'DEFAULT::PANELWALKPOSE', 'field': 'DEFAULT::panelwalkpose', 'state': True, 'parentclass': 'DEFAULT', 'groups': [], 'parameters': [{'srcline': 14, 'description': 'x of goal location', 'max': 10.0, 'cconsttype': 'const double', 'ctype': 'double', 'srcfile': '/home/whrl/indigo_ws/src/space_robotics_challenge/val_tasks/val_task1/cfg/task1_parameters.cfg', 'name': 'x', 'edit_method': '', 'default': 2.828, 'level': 0, 'min': 0.0, 'type': 'double'}, {'srcline': 15, 'description': 'y of goal location', 'max': 1.0, 'cconsttype': 'const double', 'ctype': 'double', 'srcfile': '/home/whrl/indigo_ws/src/space_robotics_challenge/val_tasks/val_task1/cfg/task1_parameters.cfg', 'name': 'y', 'edit_method': '', 'default': 0.292, 'level': 0, 'min': -1.0, 'type': 'double'}, {'srcline': 16, 'description': 'theta of goal location', 'max': 1.57, 'cconsttype': 'const double', 'ctype': 'double', 'srcfile': '/home/whrl/indigo_ws/src/space_robotics_challenge/val_tasks/val_task1/cfg/task1_parameters.cfg', 'name': 'theta', 'edit_method': '', 'default': 0.82, 'level': 0, 'min': -1.57, 'type': 'double'}], 'type': '', 'id': 1}], 'parameters': [], 'type': '', 'id': 0}

min = {}
max = {}
defaults = {}
level = {}
type = {}
all_level = 0

#def extract_params(config):
#    params = []
#    params.extend(config['parameters'])    
#    for group in config['groups']:
#        params.extend(extract_params(group))
#    return params

for param in extract_params(config_description):
    min[param['name']] = param['min']
    max[param['name']] = param['max']
    defaults[param['name']] = param['default']
    level[param['name']] = param['level']
    type[param['name']] = param['type']
    all_level = all_level | param['level']

