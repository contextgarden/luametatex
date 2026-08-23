/*
    See license.txt in the root of this project.
*/

# ifndef LMT_UTILITIES_KDTREE_2D_H
# define LMT_UTILITIES_KDTREE_2D_H

typedef struct kd2_point_data *kd2_point;

typedef struct kd2_point_data {
    double x;
    double y;
    /* we could pack these in one int */
    int    index;
    int    axis;
} kd2_point_data;

typedef struct kd2_node_data *kd2_node;

typedef struct kd2_node_data {
    kd2_node       left;
    kd2_node       right;
    kd2_point_data point;
} kd2_node_data;

kd2_node kd2_insert       (kd2_node root, kd2_point p);
kd2_node kd2_delete       (kd2_node root, kd2_point p);
kd2_node kd2_nearest      (kd2_node root, kd2_point p);
kd2_node kd2_nearestindex (kd2_node root, kd2_point p);
kd2_node kd2_found        (kd2_node root, kd2_point p);
void     kd2_flush        (kd2_node root);

# endif