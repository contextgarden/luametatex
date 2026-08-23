/*
    See license.txt in the root of this project.
*/

# include <stdio.h>
# include <stdlib.h>
# include <math.h>

# include "auxkdtree3d.h"
# include "auxmemory.h"

/*tex

    This started out as a small library embedded in lmtkdtreelib but at some point I decided to
    make two specialized ones instead. This is one of the things an llm can do when you feed it
    a proper file. This kind of kdtree code is after all rather well known. In the process I
    removed some iterative variants and passing a \quote {management} structure that kept track
    of the used dimensions. We also avoid modulo operations on the dimension.

*/

# define epsilon 1e-9

static inline double kd3_get(kd3_point p, int dim)
{
    return (dim == 0) ? p->x : ((dim == 1) ? p->y : p->z);
}

static inline int kd3_equal(kd3_point a, kd3_point b)
{
    return
        (fabs(a->x - b->x) < epsilon) &&
        (fabs(a->y - b->y) < epsilon) &&
        (fabs(a->z - b->z) < epsilon);
}

static inline int kd3_exact(kd3_point a, kd3_point b)
{
    return (a->x == b->x) && (a->y == b->y) && (a->z == b->z);
}

static inline double kd3_squared_distance(kd3_point a, kd3_point b)
{
    double dx = a->x - b->x;
    double dy = a->y - b->y;
    double dz = a->z - b->z;
    return (dx * dx) + (dy * dy) + (dz * dz);
}

static kd3_node kd3_create(kd3_point p)
{
    kd3_node temp = (kd3_node) lmt_memory_malloc(sizeof(kd3_node_data));
    if (temp) {
        temp->left  = NULL;
        temp->right = NULL;
        temp->point = *p;
    }
    return temp;
}

static kd3_node kd3_min_of_three(kd3_node middle, kd3_node left, kd3_node right, int target_dim)
{
    kd3_node min = middle;
    if (left && kd3_get(&(left->point), target_dim) < kd3_get(&(min->point), target_dim)) {
        min = left;
    }
    if (right && kd3_get(&(right->point), target_dim) < kd3_get(&(min->point), target_dim)) {
        min = right;
    }
    return min;
}

static kd3_node kd3_min_entry(kd3_node root, int target_dim, int current_dim)
{
    if (! root) {
        return NULL;
    } else {
        int next_dim = (current_dim == 2) ? 0 : current_dim + 1;
        if (current_dim == target_dim) {
            if (root->left) {
                return kd3_min_entry(root->left, target_dim, next_dim);
            } else {
                return root;
            }
        } else {
            return kd3_min_of_three(
                root,
                kd3_min_entry(root->left, target_dim, next_dim),
                kd3_min_entry(root->right, target_dim, next_dim),
                target_dim
            );
        }
    }
}

static kd3_node kd3_min(kd3_node root, int target_dim)
{
    return kd3_min_entry(root, target_dim, 0);
}

static kd3_node kd3_insert_entry(kd3_node root, kd3_point p, int current_dim)
{
    if (! root) {
        return kd3_create(p);
    } else {
        int next_dim = (current_dim == 2) ? 0 : current_dim + 1;
        if (kd3_get(p, current_dim) < kd3_get(&(root->point), current_dim)) {
            root->left  = kd3_insert_entry(root->left,  p, next_dim);
        } else {
            root->right = kd3_insert_entry(root->right, p, next_dim);
        }
        return root;
    }
}

kd3_node kd3_insert(kd3_node root, kd3_point p)
{
    return kd3_insert_entry(root, p, 0);
}

static kd3_node kd3_delete_entry(kd3_node root, kd3_point p, int current_dim)
{
    if (! root) {
        return NULL;
    } else {
        int next_dim = (current_dim == 2) ? 0 : current_dim + 1;
        if (kd3_equal(&(root->point), p)) {
            if (! root->left && ! root->right) {
                lmt_memory_free(root);
                return NULL;
            } else if (root->right) {
                kd3_node min = kd3_min(root->right, current_dim);
                root->point = min->point;
                root->right = kd3_delete_entry(root->right, &(min->point), next_dim);
            } else if (root->left) {
                kd3_node min = kd3_min(root->left, current_dim);
                root->point = min->point;
                root->right = kd3_delete_entry(root->left, &(min->point), next_dim);
                root->left  = NULL;
            }
        } else if (kd3_get(p, current_dim) < kd3_get(&(root->point), current_dim)) {
            root->left = kd3_delete_entry(root->left, p, next_dim);
        } else {
            root->right = kd3_delete_entry(root->right, p, next_dim);
        }
        return root;
    }
}

kd3_node kd3_delete(kd3_node root, kd3_point p)
{
    return kd3_delete_entry(root, p, 0);
}

static void kd3_nearest_entry(kd3_node root, kd3_point p, int current_dim, kd3_node *nearest, double *distance)
{
    if (root) {
        kd3_node first  = NULL;
        kd3_node second = NULL;
        double d = kd3_squared_distance(&(root->point), p);
        if (! *nearest || d < *distance) {
            *distance = d;
            *nearest  = root;
        }
     // if (kd3_get(p, current_dim) < kd3_get(&(root->point), current_dim)) {
        double axis_dist = kd3_get(p, current_dim) - kd3_get(&(root->point), current_dim);
        if (axis_dist < 0) {
            first  = root->left;
            second = root->right;
        } else {
            first  = root->right;
            second = root->left;
        }
        int next_dim = (current_dim == 2) ? 0 : current_dim + 1;
        kd3_nearest_entry(first, p, next_dim, nearest, distance);
     // double axis_dist = kd3_get(p, current_dim) - kd3_get(&(root->point), current_dim);
        axis_dist = axis_dist * axis_dist;
        if (axis_dist < *distance) {
            kd3_nearest_entry(second, p, next_dim, nearest, distance);
        }
    }
}

kd3_node kd3_nearest(kd3_node root, kd3_point p)
{
    kd3_node nearest = NULL;
    double distance = INFINITY;
    kd3_nearest_entry(root, p, 0, &nearest, &distance);
    return nearest;
}

void kd3_flush(kd3_node root)
{
    if (root) {
        kd3_flush(root->left);
        kd3_flush(root->right);
        lmt_memory_free(root);
    }
}

static kd3_node kd3_found_entry(kd3_node root, kd3_point p, int current_dim)
{
    if (! root) {
        return NULL;
    } else {
        int next_dim = (current_dim == 2) ? 0 : current_dim + 1;
        if (kd3_exact(&(root->point), p)) {
            return root;
        } else if (kd3_get(p, current_dim) < kd3_get(&(root->point), current_dim)) {
            return kd3_found_entry(root->left, p, next_dim);
        } else {
            return kd3_found_entry(root->right, p, next_dim);
        }
    }
}

kd3_node kd3_found(kd3_node root, kd3_point p)
{
    return kd3_found_entry(root, p, 0);
}

/* does this one actually makes sense */

static kd3_node kd3_nearestindex_entry(kd3_node root, kd3_point p, unsigned depth, kd3_node nearest, double * distance)
{
    if (root) {
        kd3_node first  = NULL;
        kd3_node second = NULL;
        double  dx     = p->x - root->point.x;
        double  dy     = p->y - root->point.y;
        double  dz     = p->z - root->point.z;
        double  d      = dx * dx + dy * dy + dz * dz;
        double  delta  = (root->point.axis == 1) ? dx : (root->point.axis == 2) ? dy : dz;
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
        nearest = kd3_nearestindex_entry(first, p, depth + 1, nearest, distance);
        if (delta * delta < *distance) {
            nearest = kd3_nearestindex_entry(second, p, depth + 1, nearest, distance);
        }
    }
    return nearest;
}

kd3_node kd3_nearestindex(kd3_node root, kd3_point p)
{
    double distance = INFINITY;
    return kd3_nearestindex_entry(root, p, 0, root, &distance);
}

