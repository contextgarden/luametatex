/*
    See license.txt in the root of this project.
*/

# include <stdio.h>
# include <stdlib.h>
# include <math.h>

# include "auxkdtree2d.h"
# include "auxmemory.h"

/*tex

    This started out as a small library embedded in lmtkdtreelib but at some point I decided to
    make two specialized ones instead. This is one of the things an llm can do when you feed it
    a proper file. This kind of kdtree code is after all rather well known. In the process I
    removed some iterative variants and passing a \quote {management} structure that kept track
    of the used dimensions. We also avoid modulo operations on the dimension.

*/

# define epsilon 1e-9

static inline double kd2_get(kd2_point p, int dim)
{
    return (dim == 0) ? p->x : p->y;
}

static inline int kd2_equal(kd2_point_data *a, kd2_point_data *b)
{
    return (fabs(a->x - b->x) < epsilon) && (fabs(a->y - b->y) < epsilon);
}

// static inline int kd2_exact(kd2_point_data *a, kd2_point_data *b)
// {
//     return (a->x == b->x) && (a->y == b->y);
// }

static inline double kd2_squared_distance(kd2_point_data *a, kd2_point_data *b)
{
    double dx = a->x - b->x;
    double dy = a->y - b->y;
    return (dx * dx) + (dy * dy);
}

static kd2_node kd2_create(kd2_point p)
{
    kd2_node temp = (kd2_node) lmt_memory_malloc(sizeof(kd2_node_data));
    if (temp) {
        temp->left  = NULL;
        temp->right = NULL;
        temp->point = *p;
    }
    return temp;
}

static kd2_node kd2_min_of_three(kd2_node middle, kd2_node left, kd2_node right, int target_dim)
{
    kd2_node min = middle;
    if (left && kd2_get(&(left->point), target_dim) < kd2_get(&(min->point), target_dim)) {
        min = left;
    }
    if (right && kd2_get(&(right->point), target_dim) < kd2_get(&(min->point), target_dim)) {
        min = right;
    }
    return min;
}

static kd2_node kd2_min_entry(kd2_node root, int target_dim, int current_dim)
{
    if (! root) {
        return NULL;
    } else {
        int next_dim = (current_dim == 0) ? 1 : 0;
        if (current_dim == target_dim) {
            if (root->left) {
                return kd2_min_entry(root->left, target_dim, next_dim);
            } else {
                return root;
            }
        } else {
            return kd2_min_of_three(
                root,
                kd2_min_entry(root->left, target_dim, next_dim),
                kd2_min_entry(root->right, target_dim, next_dim),
                target_dim
            );
        }
    }
}

static kd2_node kd2_min(kd2_node root, int target_dim)
{
    return kd2_min_entry(root, target_dim, 0);
}

static kd2_node kd2_insert_entry(kd2_node root, kd2_point p, int current_dim)
{
    if (! root) {
        return kd2_create(p);
    } else {
        int next_dim = (current_dim == 0) ? 1 : 0;
        if (kd2_get(p, current_dim) < kd2_get(&(root->point), current_dim)) {
            root->left  = kd2_insert_entry(root->left,  p, next_dim);
        } else {
            root->right = kd2_insert_entry(root->right, p, next_dim);
        }
        return root;
    }
}

kd2_node kd2_insert(kd2_node root, kd2_point p)
{
    return kd2_insert_entry(root, p, 0);
}

static kd2_node kd2_delete_entry(kd2_node root, kd2_point p, int current_dim)
{
    if (! root) {
        return NULL;
    } else {
        int next_dim = (current_dim == 0) ? 1 : 0;
        if (kd2_equal(&(root->point), p)) {
            if (! root->left && ! root->right) {
                lmt_memory_free(root);
                return NULL;
            } else if (root->right) {
                kd2_node min = kd2_min(root->right, current_dim);
                root->point = min->point;
                root->right = kd2_delete_entry(root->right, &(min->point), next_dim);
            } else if (root->left) {
                kd2_node min = kd2_min(root->left, current_dim);
                root->point = min->point;
                root->right = kd2_delete_entry(root->left, &(min->point), next_dim);
                root->left  = NULL;
            }
        } else if (kd2_get(p, current_dim) < kd2_get(&(root->point), current_dim)) {
            root->left = kd2_delete_entry(root->left, p, next_dim);
        } else {
            root->right = kd2_delete_entry(root->right, p, next_dim);
        }
        return root;
    }
}

kd2_node kd2_delete(kd2_node root, kd2_point p)
{
    return kd2_delete_entry(root, p, 0);
}

static void kd2_nearest_entry(kd2_node root, kd2_point p, int current_dim, kd2_node *nearest, double *distance)
{
    if (root) {
        kd2_node first  = NULL;
        kd2_node second = NULL;
        double d = kd2_squared_distance(&(root->point), p);
        if (! *nearest || d < *distance) {
            *distance = d;
            *nearest  = root;
        }
     // if (kd2_get(p, current_dim) < kd2_get(&(root->point), current_dim)) {
        double axis_dist = kd2_get(p, current_dim) - kd2_get(&(root->point), current_dim);
        if (axis_dist < 0) {
            first  = root->left;
            second = root->right;
        } else {
            first  = root->right;
            second = root->left;
        }
        int next_dim = (current_dim == 0) ? 1 : 0;
        kd2_nearest_entry(first, p, next_dim, nearest, distance);
     // double axis_dist = kd2_get(p, current_dim) - kd2_get(&(root->point), current_dim);
        axis_dist = axis_dist * axis_dist;
        if (axis_dist < *distance) {
            kd2_nearest_entry(second, p, next_dim, nearest, distance);
        }
    }
}

kd2_node kd2_nearest(kd2_node root, kd2_point p)
{
    kd2_node nearest = NULL;
    double distance = INFINITY;
    kd2_nearest_entry(root, p, 0, &nearest, &distance);
    return nearest;
}

void kd2_flush(kd2_node root)
{
    if (root) {
        kd2_flush(root->left);
        kd2_flush(root->right);
        lmt_memory_free(root);
    }
}

static kd2_node kd2_found_entry(kd2_node root, kd2_point p, int current_dim)
{
    if (! root) {
        return NULL;
    } else {
        int next_dim = (current_dim == 0) ? 1 : 0;
        if (kd2_equal(&(root->point), p)) {
            return root;
        } else if (kd2_get(p, current_dim) < kd2_get(&(root->point), current_dim)) {
            return kd2_found_entry(root->left, p, next_dim);
        } else {
            return kd2_found_entry(root->right, p, next_dim);
        }
    }
}

kd2_node kd2_found(kd2_node root, kd2_point p)
{
    return kd2_found_entry(root, p, 0);
}

static kd2_node kd2_nearestindex_entry(kd2_node root, kd2_point p, unsigned depth, kd2_node nearest, double * distance)
{
    if (root) {
        kd2_node first  = NULL;
        kd2_node second = NULL;
        double  dx      = p->x - root->point.x;
        double  dy      = p->y - root->point.y;
        double  d       = dx * dx + dy * dy;
        double  delta   = root->point.axis == 1 ? dx : dy;
        if (d < *distance) {
            *distance = d;
            nearest = root;
        }
        if (delta <= 0) {
            first  = root->left;
            second = root->right;
        } else {
            first  = root->right;
            second = root->left;
        }
        nearest = kd2_nearestindex_entry(first, p, depth + 1, nearest, distance);
        if (delta * delta < *distance) {
            nearest = kd2_nearestindex_entry(second, p, depth + 1, nearest, distance);
        }
    }
    return nearest;
}

kd2_node kd2_nearestindex(kd2_node root, kd2_point p)
{
    double distance = INFINITY;
    return kd2_nearestindex_entry(root, p, 0, root, &distance);
}
