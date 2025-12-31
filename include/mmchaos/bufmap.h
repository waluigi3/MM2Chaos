#pragma once

#include <array>
#include <optional>

namespace mmchaos {
    // Thanks to Robert Sedgewick for left leaning red black trees
    template <class Key, class V, size_t N>
    class bufmap {
        static constexpr bool RED = true;
        static constexpr bool BLACK = false;

        struct bufmap_node {
            Key k;
            V v;
            bufmap_node* left;
            bufmap_node* right;
            bool color;
        };
        using buf_t = std::array<bufmap_node, N>;
        buf_t buf;
        bufmap_node* root = nullptr;
        buf_t::iterator next_slot = buf.begin();

        bool is_red(bufmap_node* node) {
            if (node == nullptr) {
                return false;
            }

            return node->color == RED;
        }
        
        void flip_colors(bufmap_node* node) {
            node->color = !node->color;
            node->left->color = !node->left->color;
            node->right->color = !node->right->color;
        }

        bufmap_node* rotate_right(bufmap_node* node) {
            bufmap_node* x = node->left;

            node->left = x->right;
            x->right = node;
            x->color = node->color;
            node->color = RED;

            return x;
        }

        bufmap_node* rotate_left(bufmap_node* node) {
            bufmap_node* x = node->right;

            node->right = x->left;
            x->left = node;
            x->color = node->color;
            node->color = RED;

            return x;
        }

        bufmap_node* set(bufmap_node* node, const Key& k, V v) {
            if (node == nullptr) {
                bufmap_node* ret = &*next_slot++;
                ret->k = k;
                ret->v = v;
                ret->left = nullptr;
                ret->right = nullptr;
                ret->color = RED;
                return ret;
            }

            if (k == node->k) {
                node->v = v;
            } else if (k < node->k) {
                node->left = set(node->left, k, v);
            } else if (k > node->k) {
                node->right = set(node->right, k, v);
            }

            if (is_red(node->right) && !is_red(node->left)) {
                node = rotate_left(node);
            }

            if (is_red(node->left) && is_red(node->left->left)) {
                node = rotate_right(node);
            }

            if (is_red(node->left) && is_red(node->right)) {
                flip_colors(node);
            }

            return node;
        }
        public:
        void clear() {
            next_slot = buf.begin();
            root = nullptr;
        }

        void set(const Key& k, V v) {
            if (next_slot == buf.end()) {
                return;
            }

            root = set(root, k, v);
            root->color = BLACK;
        }

        std::optional<V> get(const Key& k) {
            bufmap_node* x = root;
            while (x != nullptr) {
                if (k == x->k) {
                    return std::optional(x->v);
                } else if (k < x->k) {
                    x = x->left;
                } else if (k > x->k) {
                    x = x->right;
                }
            }

            return std::optional<V>();
        }
    };
}