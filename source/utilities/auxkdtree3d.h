/*
    See license.txt in the root of this project.
*/

# ifndef LMT_UTILITIES_KDTREE_3D_H
# define LMT_UTILITIES_KDTREE_3D_H

typedef struct kd3_point_data *kd3_point;

typedef struct kd3_point_data {
    double x;
    double y;
    double z;
    /* we could pack these in one int */
    int    index;
    int    axis;
} kd3_point_data;

typedef struct kd3_node_data *kd3_node;

typedef struct kd3_node_data {
    kd3_node       left;
    kd3_node       right;
    kd3_point_data point;
} kd3_node_data;

kd3_node kd3_insert       (kd3_node root, kd3_point p);
kd3_node kd3_delete       (kd3_node root, kd3_point p);
kd3_node kd3_nearest      (kd3_node root, kd3_point p);
kd3_node kd3_nearestindex (kd3_node root, kd3_point p);
kd3_node kd3_found        (kd3_node root, kd3_point p);
void     kd3_flush        (kd3_node root);

# endif