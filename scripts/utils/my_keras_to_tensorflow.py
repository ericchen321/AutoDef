
# coding: utf-8

# # Set parameters

# In[ ]:


"""
Copyright (c) 2017, by the Authors: Amir H. Abdi
This software is freely available under the MIT Public License. 
Please see the License file in the root for details.

The following code snippet will convert the keras model file,
which is saved using model.save('kerasmodel_weight_file'),
to the freezed .pb tensorflow weight file which holds both the 
network architecture and its associated weights.
""";


# In[ ]:


# setting input arguments
# import argparse
# parser = argparse.ArgumentParser(description='set input arguments')
# parser.add_argument('-input_fld', action="store", 
#                     dest='input_fld', type=str, default='.')

# parser.add_argument('-output_fld', action="store", 
#                     dest='output_fld', type=str, default='.')

# parser.add_argument('-input_model_file', action="store", 
#                     dest='input_model_file', type=str, default='model.h5')

# parser.add_argument('-output_model_file', action="store", 
#                     dest='output_model_file', type=str, default='model.pb')

# parser.add_argument('-output_graphdef_file', action="store", 
#                     dest='output_graphdef_file', type=str, default='model.ascii')

# parser.add_argument('-num_outputs', action="store", 
#                     dest='num_outputs', type=int, default=1)

# parser.add_argument('-graph_def', action="store", 
#                     dest='graph_def', type=bool, default=False)

# parser.add_argument('-output_node_prefix', action="store", 
#                     dest='output_node_prefix', type=str, default='output_node')

# parser.add_argument('-f')
# args = parser.parse_args()
# print('input args: ', args)


# In[ ]:


# uncomment the following lines to alter the default values set above
# args.input_fld = '.'
# args.output_fld = '.'
# args.input_model_file = 'model.h5'
# args.output_model_file = 'model.pb'

# num_output: this value has nothing to do with the number of classes, batch_size, etc., 
# and it is mostly equal to 1. 
# If you have a multi-stream network (forked network with multiple outputs), 
# set the value to the number of outputs.

# if not os.path.isdir(output_fld):
#     os.mkdir(output_fld)
# weight_file_path = osp.join(args.input_fld, args.input_model_file)


# # Load keras model and rename output

# In[ ]:

def save_keras_model_as_tf(keras_model, output_path):
    num_output = 1#args.num_outputs


    # # initialize

    # In[ ]:


    from keras.models import load_model
    import tensorflow as tf
    import os
    import os.path as osp
    from keras import backend as K

    output_fld =  '.'
    K.set_learning_phase(0)
    net_model = keras_model

    pred = [None]*num_output
    pred_node_names = [None]*num_output
    for i in range(num_output):
        pred_node_names[i] = 'output_node'+str(i)
        pred[i] = tf.identity(net_model.outputs[i], name=pred_node_names[i])
    print('output nodes names are: ', pred_node_names)


    # #### [optional] write graph definition in ascii

    # In[ ]:


    sess = K.get_session()

    # if args.graph_def:
    #     f = args.output_graphdef_file 
    #     tf.train.write_graph(sess.graph.as_graph_def(), output_fld, f, as_text=True)
    #     print('saved the graph definition in ascii format at: ', osp.join(output_fld, f))


    # #### convert variables to constants and save

    # In[ ]:


    from tensorflow.python.framework import graph_util
    from tensorflow.python.framework import graph_io
    print("pred_node_names", pred_node_names)
    constant_graph = graph_util.convert_variables_to_constants(sess, sess.graph.as_graph_def(), pred_node_names)
    # print([n.attr for n in constant_graph.node])
    # print([n.op for n in constant_graph.node])
    # print([n.input for n in constant_graph.node])
    graph_io.write_graph(constant_graph, output_fld, output_path, as_text=False)
    print('saved the freezed graph (ready for inference) at: ', osp.join(output_fld, output_path))

