import sys
import subprocess
import os
import json
import tempfile
import shutil

from utils import my_utils
from build_model import build_model

# Requirements
# 1. Generate record user interaction
# 2. automatically replay on high res mesh
# 3. Train model afterwards
#
# Want to be able to rerun training
# Solution -> Output generated configs to the model folder

SOLVER_BIN_PATH = "reduced_sim_gauss/build/bin/ReduceSimExample"


def main():
    config_path = sys.argv[1]
    model_path = sys.argv[2]

    # confirm overwrite
    my_utils.delete_and_recreate_dir_with_confirmation(model_path)

    # Load unified config
    unified_config = {}
    with open(config_path, 'r') as f:
        unified_config = json.load(f)

    # --- Record User Interaction ---
    log_name = 'user_interaction_log'
    low_res_model_path, _ = setup_submodel(model_path, unified_config, 'low_res')
    subprocess.run([SOLVER_BIN_PATH, low_res_model_path, '--log_name', log_name])

    
    # --- Replay on high res model ---
    user_interaction_log_path = os.path.join(low_res_model_path, 'simulation_logs/', log_name, 'sim_stats.json')
    high_res_model_path, training_data_ouput_path = setup_submodel(model_path, unified_config, 'high_res')
    subprocess.run([SOLVER_BIN_PATH, high_res_model_path, user_interaction_log_path, '--no_wait'])

    # # Make training config
    learning_config = create_learning_config(unified_config, training_data_ouput_path)

    build_model(learning_config, model_path, force=True)


def setup_submodel(model_path, unified_config, type_str): # type_str = 'low_res' or 'high_res'
    submodel_path = os.path.join(model_path, 'unified_generate/' + type_str + '/')
    my_utils.create_dir_if_not_exist(submodel_path)

    # Copy in the mesh
    shutil.copy(unified_config[type_str + '_mesh'], os.path.join(submodel_path, 'tets.mesh'))

    sim_config, training_data_ouput_path = create_sim_config(submodel_path, unified_config, type_str)
    sim_config_path = os.path.join(submodel_path, 'sim_config.json')
    with open(sim_config_path, 'w') as f:
        json.dump(sim_config, f, indent=2)

    return submodel_path, training_data_ouput_path


def create_sim_config(submodel_path, unified_config, type_str):
    sim_params = unified_config['sim_params']

    if type_str == 'low_res':
        mesh_path = unified_config['low_res_mesh']
        youngs_modulus = unified_config['sim_params']['low_res_youngs_modulus']
        training_data_ouput_path = ''
    elif type_str == 'high_res':
        mesh_path = unified_config['high_res_mesh']
        youngs_modulus = unified_config['sim_params']['high_res_youngs_modulus']
        training_data_ouput_path = os.path.join(submodel_path, 'training_data/')

    sim_config = {
        'mesh': mesh_path, # TODO this might need to be relative to solver?
        'logging_enabled': True,
        'save_objs': False,
        'save_training_data': training_data_ouput_path != '',
        'save_training_data_path': training_data_ouput_path,
        'alternative_full_space_mesh': '',
        'material_config': {
            'density': sim_params['density'],
            'youngs_modulus': youngs_modulus,
            'poissons_ratio': sim_params['poissons_ratio'],
        },

        'integrator_config': {
            'reduced_space_type': 'full',
            'use_reduced_energy': False,
            'use_partial_decode': False,
            'reduced_energy_method': 'full',
            'use_preconditioner': True,
            'pca_dim': 0,
            'ae_encoded_dim': 0, 
            'ae_decoded_dim': 0,
            'timestep': sim_params['timestep'],
            'lbfgs_config': {
                'lbfgs_max_iterations': sim_params['lbfgs_max_iterations'],
                'lbfgs_epsilon': sim_params['lbfgs_epsilon'],
                'lbfgs_delta': sim_params['lbfgs_delta'],
                'lbfgs_delta_past': sim_params['lbfgs_delta_past'],
                'lbfgs_m': sim_params['lbfgs_m'],
            },
            'gravity': sim_params['gravity'],
            'gravity_axis': sim_params['gravity_axis'],
            'start_pose_from_training_data': -1,
            'quasi_static': False,
            'save_obj_every_iteration': False,
        },

        'visualization_config' : {
            'gpu_decode': False,
            'show_stress': False,
            'show_energy': False,
            'interaction_spring_stiffness': sim_params['interaction_spring_stiffness'],
            'spring_grab_radius': sim_params['spring_grab_radius'],
            'full_space_constrained_axis': sim_params['full_space_constrained_axis'],
            'constrained_axis_eps': sim_params['constrained_axis_eps'],
            'flip_constrained_axis': sim_params['flip_constrained_axis'],
            'fixed_point_constraint': sim_params['fixed_point_constraint'],
            'fixed_point_radius': sim_params['fixed_point_radius'],
            'print_every_n_frames': 1,
            'max_frames': 0,
        },
    }

    return sim_config, training_data_ouput_path


def create_learning_config(unified_config, training_data_ouput_path):
    training_params = unified_config['training_params']

    learning_config = {
        "training_dataset": training_data_ouput_path,
        "validation_dataset": "",

        "learning_config": {
            "save_objs": False,
            "skip_training": False,
            "skip_jacobian": False,
            "use_mass_pca": False,
            "record_full_loss": False,
            "autoencoder_config": {
                "pca_compare_dims": [training_params['ae_encoded_dim']],
                "pca_layer_err": training_params['pca_max_vert_error_meters'],
                "train_in_full_space": False,
                "pca_init": True,
                "non_pca_layer_sizes": training_params['non_pca_layer_sizes'],
                "ae_encoded_dim": training_params['ae_encoded_dim'],
                "activation": training_params['activation'],

                "learning_rate": training_params['learning_rate'],
                "batch_size": training_params['batch_size'],
                "training_epochs": training_params['training_epochs'],
                "do_fine_tuning": False,
                "loss_weight": 1.0
            },
            "energy_model_config": {
                "type": training_params['cubature_type'],
                "enabled": True,
                "pca_dim": 40,
                "num_sample_tets": training_params['num_sample_tets'],
                "brute_force_iterations": 100,
                "target_anneal_mins": 5
            }
        }
    }

    return learning_config


if __name__ == '__main__':
    main()