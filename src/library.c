/* Name, library.c, CS 24000, Fall 2020
 * Last updated October 12, 2020
 */

#include "library.h"

#ifdef CUSTOM_MALLOC
#include <malloc.h>
#else
#include <stdlib.h>
#endif
#include <string.h>
#include <ftw.h>
#include <assert.h>

tree_node_t *g_song_library;


//  Tree operations
tree_node_t **find_parent_pointer(tree_node_t ** head, const char * song_name){
    if(*head == NULL) return NULL;
    int result = strcmp((*head)->song_name, song_name);
    if(result == 0)
        return head;

    if(result > 0)
        return find_parent_pointer(&(*head)->left_child, song_name);

    return find_parent_pointer(&(*head)->right_child, song_name);
}

int tree_insert(tree_node_t ** head, tree_node_t *test){
    tree_node_t *current_node = *head;
    int result;
    tree_node_t *prev_node = NULL;
    if(find_parent_pointer(head, test->song_name))
        return DUPLICATE_SONG;

    if(*head == NULL)
    {
        *head = test;
        return INSERT_SUCCESS;
    }

    while(current_node != NULL) {
        prev_node = current_node;
        result = strcmp(current_node->song_name, test->song_name);
        if(result == 0) return DUPLICATE_SONG;
        else if(result > 0) current_node = current_node->left_child;
        else current_node = current_node->right_child;
    }

    if (strcmp(prev_node->song_name, test->song_name) > 0)
    {
        prev_node->left_child = test;
    }
    else {
        prev_node->right_child = test;
    }

    return INSERT_SUCCESS;
}

int remove_song_from_tree(tree_node_t ** head, const char * song_name) {
    tree_node_t ** curr = head;

    curr = find_parent_pointer(head, song_name);
    if(curr == NULL)
        return SONG_NOT_FOUND;

    tree_node_t *left = (*curr)->left_child;
    tree_node_t *right = (*curr)->right_child;
    free_node(*curr);
    *curr = NULL;

    if(right != NULL) {
        tree_insert(head, right);
    }
    if(left != NULL) {
        tree_insert(head, left);
    }

    return DELETE_SUCCESS;
}

void free_node(tree_node_t * node) {
    if(node == NULL) return;

    free_song(node->song);
    free(node);
}

void print_node(tree_node_t * node, FILE * fp){
    fprintf(fp, "%s\n", node->song_name);
}

//  Traversal functions
void traverse_pre_order(tree_node_t * node, void *ptr, traversal_func_t callback) {
    if(node == NULL) return;

    callback(node, ptr);
    traverse_pre_order(node->left_child, ptr, callback);
    traverse_pre_order(node->right_child, ptr, callback);
}

void traverse_in_order(tree_node_t * node, void *ptr, traversal_func_t callback) {
    if(node == NULL) return;

    traverse_in_order(node->left_child, ptr, callback);
    callback(node, ptr);
    traverse_in_order(node->right_child, ptr, callback);
}

void traverse_post_order(tree_node_t * node, void *ptr, traversal_func_t callback) {
    if(node == NULL) return;

    traverse_post_order(node->left_child, ptr, callback);
    traverse_post_order(node->right_child, ptr, callback);
    callback(node, ptr);
}

void free_library(tree_node_t * node){
    if(node == NULL) return;

    free_library(node->left_child);
    free_library(node->right_child);
    free_node(node);
}

void write_song_list(FILE *fp, tree_node_t * node){
    if(node == NULL) return;

    write_song_list(fp, node->left_child);
    print_node(node, fp);
    write_song_list(fp, node->right_child);
}

static
int ftw_callback(const char*fpath, const struct stat* sb,
        int typeflag) {
    size_t len = strlen(strrchr(fpath, '/') + 1);
    char* filename = (char*)malloc(len + 1);
    strcpy(filename, strrchr(fpath, '/') + 1);
    if(typeflag == FTW_F) {
        char* value = strrchr(filename, '.') + 1;
        if(strcmp(value, "mid") == 0) {
            tree_node_t* node = (tree_node_t*)malloc(sizeof(tree_node_t));
            node->song_name = filename;
            node->song = parse_file(fpath);
            node->right_child = NULL;
            node->left_child = NULL;
            assert(DUPLICATE_SONG != tree_insert(&g_song_library, node));
        }
    }
    return 0;
}

//  Data type specific
void
make_library(const char *path) {
    if (ftw(path, ftw_callback, 5) != 0)
        printf("Error on ftw path : %s\n", path);
}