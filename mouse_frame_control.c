#include <yed/plugin.h>

static int                if_dragging_resize;
static int                if_dragging_resize_b;
static int                if_dragging_move;
static yed_frame         *drag_frame;
static u64                last_timestamp;
static int                box_h, box_w;
static int                mouse_loc_r;
static int                mouse_loc_c;
static int                no_more;
static int                on_boarder;
static yed_direct_draw_t *dd;
static yed_frame         *left_frame;

static void       mouse(yed_event* event);
static void       mouse_unload(yed_plugin *self);
static yed_frame *find_frame(yed_event* event);
static int        yed_cell_is_in_frame_mouse(int row, int col, yed_frame *frame);
static int        yed_cell_is_in_frame_tree_mouse(int row, int col, yed_frame_tree *frame_tree);
static int        is_in_bottom_right(yed_frame* frame, yed_event* event);
static int        is_on_boarder(yed_frame* frame, yed_event* event);
static void       middle_click(yed_event* event);
static void       middle_drag(yed_event* event);
static void       middle_release(yed_event* event);
static void       draw_corner(yed_event* event);
static void       draw(void);
static void       check_for_left(yed_frame_tree *frame_tree, void *event);

int yed_plugin_boot(yed_plugin *self) {
    yed_plugin_request_mouse_reporting(self);

    YED_PLUG_VERSION_CHECK();

    yed_event_handler mouse_eh;
    mouse_eh.kind = EVENT_KEY_PRESSED;
    mouse_eh.fn   = mouse;
    yed_plugin_add_event_handler(self, mouse_eh);

    yed_event_handler corner_eh;
    corner_eh.kind = EVENT_FRAME_ACTIVATED;
    corner_eh.fn   = draw_corner;
    yed_plugin_add_event_handler(self, corner_eh);

    yed_plugin_set_unload_fn(self, mouse_unload);

    box_h                = 5;
    box_w                = 5;
    mouse_loc_r          = 0;
    mouse_loc_c          = 0;
    if_dragging_resize   = 0;
    if_dragging_resize_b = 0;
    if_dragging_move     = 0;
    no_more              = 0;

    return 0;
}

int yed_cell_is_in_frame_mouse(int row, int col, yed_frame *frame) {
    return    (row >= frame->btop  && row <= frame->btop  + frame->bheight - 1)
           && (col >= frame->bleft && col <= frame->bleft + frame->bwidth  - 1);
}

int yed_cell_is_in_frame_tree_mouse(int row, int col, yed_frame_tree *frame_tree) {
    float           top;
    float           left;
    float           width;
    float           height;

    yed_frame_tree_get_absolute_rect(frame_tree, &top, &left, &height, &width);

    return (row >= (top * (ys->term_rows -2))                                   &&
            row <= (top * (ys->term_rows -2)) + (height * ys->term_rows -2) - 1 &&
            col >= (left * ys->term_cols)                                       &&
            col <= (left * ys->term_cols) + (width * ys->term_cols) - 1);
}

//see if cursor is inside of active frame
//in reverse order find first one that the cursor is inside
yed_frame *find_frame(yed_event* event) {
    yed_frame **frame_it;

    if (yed_cell_is_in_frame_mouse(MOUSE_ROW(event->key), MOUSE_COL(event->key), ys->active_frame)) {
        return ys->active_frame;
    }

    array_rtraverse(ys->frames, frame_it) {
        if (yed_cell_is_in_frame_mouse(MOUSE_ROW(event->key), MOUSE_COL(event->key), (*frame_it))) {
            return (*frame_it);
        }
    }

    return NULL;
}

static int is_in_bottom_right(yed_frame* frame, yed_event* event) {
    float           top;
    float           left;
    float           width;
    float           height;
    yed_frame_tree *frame_tree;

    if (frame == NULL) {return 0;}

    frame_tree = yed_frame_tree_get_root(frame->tree);
    yed_frame_tree_get_absolute_rect(frame_tree, &top, &left, &height, &width);

    if (MOUSE_ROW(event->key) <= (height * (ys->term_rows - 2)) + (top * (ys->term_rows - 2))         &&
        MOUSE_ROW(event->key) >= (height * (ys->term_rows - 2)) + (top * (ys->term_rows - 2)) - box_h &&
        MOUSE_COL(event->key) <= (width * ys->term_cols) + (left * ys->term_cols)                     &&
        MOUSE_COL(event->key) >= (width * ys->term_cols) + (left * ys->term_cols) - box_w) {
        return 1;
    }else{
        return 0;
    }
}

static int is_on_boarder(yed_frame* frame, yed_event* event) {
    float           top;
    float           left;
    float           width;
    float           height;
    yed_frame_tree *frame_tree;

    if (frame == NULL) {return 0;}
    if (yed_frame_tree_is_root(frame->tree)) {return 0;}

    frame_tree = yed_frame_tree_get_root(frame->tree);
    yed_frame_tree_get_absolute_rect(frame_tree, &top, &left, &height, &width);


    if ((MOUSE_ROW(event->key) <= MAX(1, (top * (ys->term_rows - 2))))        ||   //top
        (MOUSE_ROW(event->key) >= ((height + top) * (ys->term_rows - 2)) - 1) ||   //bottom
        (MOUSE_COL(event->key) <= MAX(1, (left * ys->term_cols)))             ||   //left
        (MOUSE_COL(event->key) >= ((width + left) * ys->term_cols) - 1)) {         //right
        return 0;
    }

/*     LOG_FN_ENTER(); */
/*     yed_log("Check\n"); */
/*     yed_log("current_row:%d  =           top:%d\n", MOUSE_ROW(event->key), frame->btop); */
/*     yed_log("current_row:%d  =        bottom:%d\n", MOUSE_ROW(event->key), frame->btop + frame->bheight - 1); */
/*     yed_log("current_row:%d <=    frame->top:%f\n", MOUSE_ROW(event->key), MAX(1, (top * (ys->term_rows - 2)))); */
/*     yed_log("current_row:%d >= frame->bottom:%f\n", MOUSE_ROW(event->key), ((height + top) * (ys->term_rows - 2) - 1)); */
/*     yed_log("current_col:%d  =          left:%d\n", MOUSE_COL(event->key), frame->bleft); */
/*     yed_log("current_col:%d  =         right:%d\n", MOUSE_COL(event->key), frame->bleft + frame->bwidth - 1); */
/*     yed_log("current_col:%d <=   frame->left:%f\n", MOUSE_COL(event->key), MAX(1, (left * ys->term_cols))); */
/*     yed_log("current_col:%d >=  frame->right:%f\n", MOUSE_COL(event->key), ((width + left) * ys->term_cols - 1)); */
/*     LOG_EXIT(); */

    if (MOUSE_ROW(event->key) ==  frame->btop                        ||
        MOUSE_ROW(event->key) == (frame->btop + frame->bheight - 1)) {
        return 1;
    }else if (MOUSE_COL(event->key) ==  frame->bleft                       ||
              MOUSE_COL(event->key) == (frame->bleft + frame->bwidth - 1)) {
        return 2;
    }else {
        return 0;
    }
}

static void middle_click(yed_event* event) {
    yed_frame *frame;
    yed_frame_tree *frame_tree;

    frame = find_frame(event);
    if (frame == NULL) {
        return;
    }
    yed_activate_frame(frame);

    mouse_loc_r = MOUSE_ROW(event->key);
    mouse_loc_c = MOUSE_COL(event->key);
    left_frame  = NULL;

    on_boarder = is_on_boarder(frame, event);
    LOG_FN_ENTER();
    if (on_boarder) {
        if (frame->tree->parent != NULL) {
/*             if (frame->tree->parent->child_trees[0] == frame->tree) { */
/*                 left_frame = frame; */
/*             }else{ */
                frame_tree = frame->tree;
                if (on_boarder == 1) {
                    BACK:;
                    while (frame_tree->parent != NULL && frame_tree->parent->split_kind != FTREE_HSPLIT) {
                        frame_tree = frame_tree->parent;
                    }
                    yed_frame_tree_leaves_do(frame_tree->parent->child_trees[0], check_for_left, event);
                    if (left_frame == NULL) {
                        if (frame_tree->parent != NULL) {
                            frame_tree = frame_tree->parent;
                            goto BACK;
                        }
                    }
                }else if (on_boarder == 2) {
                    while (frame_tree->parent != NULL && frame_tree->parent->split_kind != FTREE_VSPLIT) {
                        frame_tree = frame_tree->parent;
                    }
                    yed_frame_tree_leaves_do(frame_tree->parent->child_trees[0], check_for_left, event);
                }
/*             } */
        }
    }
    LOG_EXIT();

    event->cancel = 1;
}

static void check_for_left(yed_frame_tree *frame_tree, void *event) {
    float           top;
    float           left;
    float           width;
    float           height;
    yed_event      *event1;
    yed_frame      *frame;

    event1 = event;
    frame  = frame_tree->frame;
    yed_frame_tree_get_absolute_rect(yed_frame_tree_get_root(frame_tree), &top, &left, &height, &width);

    LOG_FN_ENTER();
    yed_log("Check\n");
    yed_log("current_row:%d  =           top:%d\n", MOUSE_ROW(event1->key), frame->btop);
    yed_log("current_row:%d  =        bottom:%d\n", MOUSE_ROW(event1->key), frame->btop + frame->bheight - 1);
    yed_log("current_row:%d <=    frame->top:%f\n", MOUSE_ROW(event1->key), MAX(1, (top * (ys->term_rows - 2))));
    yed_log("current_row:%d >= frame->bottom:%f\n", MOUSE_ROW(event1->key), ((height + top) * (ys->term_rows - 2) - 1));
    yed_log("current_col:%d  =          left:%d\n", MOUSE_COL(event1->key), frame->bleft);
    yed_log("current_col:%d  =         right:%d\n", MOUSE_COL(event1->key), frame->bleft + frame->bwidth - 1);
    yed_log("current_col:%d <=   frame->left:%f\n", MOUSE_COL(event1->key), MAX(1, (left * ys->term_cols)));
    yed_log("current_col:%d >=  frame->right:%f\n", MOUSE_COL(event1->key), ((width + left) * ys->term_cols - 1));
    LOG_EXIT();

    LOG_FN_ENTER();
    if ((MOUSE_ROW(event1->key) <= MAX(1, (top * (ys->term_rows - 2))))        ||   //top
        (MOUSE_ROW(event1->key) >= ((height + top) * (ys->term_rows - 2)) - 1) ||   //bottom
        (MOUSE_COL(event1->key) <= MAX(1, (left * ys->term_cols)))             ||   //left
        (MOUSE_COL(event1->key) >= ((width + left) * ys->term_cols) - 1)) {         //right
        yed_log("outer edge\n");
        LOG_EXIT();
        return;
    }

    if (MOUSE_ROW(event1->key) ==  frame->btop                        ||
        MOUSE_ROW(event1->key) == (frame->btop + frame->bheight - 1)) {
            if (MOUSE_COL(event1->key) >= frame->bleft && MOUSE_COL(event1->key) <= (frame->bleft + frame->bwidth - 1)) {
                yed_log("on top/bottom within left/right\n");
                goto LEFT;
            }else{
                yed_log("on top/bottom not within left/right\n");
                LOG_EXIT();
                return;
            }
    }else if (MOUSE_COL(event1->key) ==  frame->bleft                       ||
              MOUSE_COL(event1->key) == (frame->bleft + frame->bwidth - 1)) {
            if (MOUSE_ROW(event1->key) >= frame->btop && MOUSE_ROW(event1->key) <= (frame->btop + frame->bheight - 1)) {
                yed_log("on left/right within top/bottom\n");
                goto LEFT;
            }else{
                yed_log("on left/right not within top/bottom\n");
                LOG_EXIT();
                return;
            }
    }else{
        yed_log("not on left/right or top/bottom\n");
        LOG_EXIT();
        return;
    }

    LEFT:;
    if (frame_tree == frame_tree->parent->child_trees[0]) {
        yed_log("set left_frame\n");
        LOG_EXIT();
        left_frame = frame_tree->frame;
    }
}

static void middle_drag(yed_event* event) {
    yed_frame      *frame;
    yed_frame_tree *frame_tree;
    int             save;
    float           unit_x, unit_y;
    float           top;
    float           left;
    float           width;
    float           height;

    frame = ys->active_frame;

    if (if_dragging_resize || is_in_bottom_right(frame, event)) {
/*         resize frame */
        if (if_dragging_resize == 0) {
            if_dragging_resize = 1;
            mouse_loc_r = MOUSE_ROW(event->key);
            mouse_loc_c = MOUSE_COL(event->key);
        }else {
            if (yed_frame_tree_is_root(frame->tree)) {
                yed_resize_frame(frame, MOUSE_ROW(event->key) - mouse_loc_r, MOUSE_COL(event->key) - mouse_loc_c);
            }else {
                frame_tree = yed_frame_tree_get_root(frame->tree);
                frame_tree = yed_frame_tree_get_split_leaf_prefer_right_or_bottommost(frame_tree->child_trees[0]);
                if (frame == frame_tree->frame) {
                    yed_resize_frame_tree(frame->tree, MOUSE_ROW(event->key) - mouse_loc_r, MOUSE_COL(event->key) - mouse_loc_c);
                }
            }

            mouse_loc_r = MOUSE_ROW(event->key);
            mouse_loc_c = MOUSE_COL(event->key);
        }
    }else if (if_dragging_resize_b || (on_boarder && if_dragging_resize_b == 0)) {
        if (if_dragging_resize_b == 0) {
            if_dragging_resize_b = 1;
        }

        if (left_frame != NULL) {
            if (on_boarder == 1) {
                yed_resize_frame(left_frame, MOUSE_ROW(event->key) - mouse_loc_r, 0);
            }else if (on_boarder == 2) {
                yed_resize_frame(left_frame, 0, MOUSE_COL(event->key) - mouse_loc_c);
            }
            mouse_loc_r = MOUSE_ROW(event->key);
            mouse_loc_c = MOUSE_COL(event->key);
        }
    }else if(yed_cell_is_in_frame_tree_mouse(MOUSE_ROW(event->key), MOUSE_COL(event->key), yed_frame_tree_get_root(frame->tree))){
/*         move frame */
        if (if_dragging_move == 0) {
            if_dragging_move = 1;
            mouse_loc_r = MOUSE_ROW(event->key);
            mouse_loc_c = MOUSE_COL(event->key);
        }else {
/*             update height */
/*             MOVE DOWN */
            if (MOUSE_ROW(event->key) > mouse_loc_r) {
                frame_tree = yed_frame_tree_get_root(frame->tree);
                yed_frame_tree_get_absolute_rect(frame_tree, &top, &left, &height, &width);

                if ((top * (ys->term_rows - 2) <= 1) && (width * ys->term_cols >= ys->term_cols)) {
                    yed_resize_frame_tree(frame_tree, (0.60 * ys->term_rows) - (height * ys->term_rows), (0.60 * ys->term_cols) - (width * ys->term_cols));
                    yed_move_frame_tree(frame_tree, 0, (0.20 * ys->term_cols) - (left * ys->term_cols));
                }

                yed_move_frame(frame, MOUSE_ROW(event->key) - mouse_loc_r, 0);

/*             MOVE UP */
            }else if (MOUSE_ROW(event->key) < mouse_loc_r) {
                yed_move_frame(frame, MOUSE_ROW(event->key) - mouse_loc_r, 0);

                if (MOUSE_ROW(event->key) == 1) {
                    frame_tree = yed_frame_tree_get_root(frame->tree);
                    yed_frame_tree_set_relative_rect(frame_tree, 0, 0, 1, 1);
                    yed_frame_tree_recursive_readjust(frame_tree);
                }
            }
            mouse_loc_r = MOUSE_ROW(event->key);

/*             update width */
/*             MOVE RIGHT */
            if (MOUSE_COL(event->key) > mouse_loc_c) {
                frame_tree = yed_frame_tree_get_root(frame->tree);
                yed_frame_tree_get_absolute_rect(frame_tree, &top, &left, &height, &width);

                if (((top * (ys->term_rows - 2) > 1) || (width * ys->term_cols < ys->term_cols)) &&
                     (left * (ys->term_cols) <= 1) && (height * (ys->term_rows - 2) == (ys->term_rows - 2))) {
                    yed_resize_frame_tree(frame_tree, (0.60 * ys->term_rows) - (height * ys->term_rows), (0.60 * ys->term_cols) - (width * ys->term_cols));
                    yed_move_frame_tree(frame_tree, (0.20 * (ys->term_rows - 2)) - (top * (ys->term_rows - 2)), 0);
                }
                yed_move_frame(frame, 0, MOUSE_COL(event->key) - mouse_loc_c);

                if (MOUSE_COL(event->key) == ys->term_cols - 2) {
                    frame_tree = yed_frame_tree_get_root(frame->tree);
                    yed_frame_tree_set_relative_rect(frame_tree, 0.0, 0.5, 1.0, 0.5);
                    yed_frame_tree_recursive_readjust(frame_tree);
                }
/*             MOVE LEFT */
            }else if (MOUSE_COL(event->key) < mouse_loc_c) {
                frame_tree = yed_frame_tree_get_root(frame->tree);
                yed_frame_tree_get_absolute_rect(frame_tree, &top, &left, &height, &width);

                if (((top * (ys->term_rows - 2) > 1) || (width * ys->term_cols < ys->term_cols)) &&
                     ((width * ys->term_cols) + (left * ys->term_cols) >= ys->term_cols) &&
                     (height * (ys->term_rows - 2) == (ys->term_rows - 2))) {
                    yed_resize_frame_tree(frame_tree, (0.60 * ys->term_rows) - (height * ys->term_rows), (0.60 * ys->term_cols) - (width * ys->term_cols));
                    yed_move_frame_tree(frame_tree, (0.20 * (ys->term_rows - 2)) - (top * (ys->term_rows - 2)), (0.40 * ys->term_cols) - (left * ys->term_cols));
                }
                yed_move_frame(frame, 0, MOUSE_COL(event->key) - mouse_loc_c);

                if (MOUSE_COL(event->key) == 1) {
                    frame_tree = yed_frame_tree_get_root(frame->tree);
                    yed_frame_tree_set_relative_rect(frame_tree, 0.0, 0.0, 1.0, 0.5);
                    yed_frame_tree_recursive_readjust(frame_tree);
                }
            }
            mouse_loc_c = MOUSE_COL(event->key);
        }
    }
    draw();
    event->cancel = 1;
}

static void middle_release(yed_event* event) {
    event->cancel        = 1;
    if_dragging_resize   = 0;
    if_dragging_resize_b = 0;
    if_dragging_move     = 0;
    mouse_loc_r          = 0;
    mouse_loc_c          = 0;
    no_more              = 0;
}

static void mouse(yed_event* event) {
    if (ys->active_frame == NULL) {return;}

    if (IS_MOUSE(event->key)) {
        switch (MOUSE_BUTTON(event->key)) {
            case MOUSE_BUTTON_MIDDLE:
                if (MOUSE_KIND(event->key) == MOUSE_PRESS) {
                    middle_click(event);

                }else if (MOUSE_KIND(event->key) == MOUSE_DRAG) {
                    middle_drag(event);

                }else if(MOUSE_KIND(event->key) == MOUSE_RELEASE) {
                    middle_release(event);
                }
                break;
        }
    }
}

static void draw_corner(yed_event* event) {
    draw();
}

static void draw() {
    yed_frame* frame;

    frame = ys->active_frame;
    if (frame == NULL) {return;}

    if (dd != NULL) {
        yed_kill_direct_draw(dd);
    }
    dd = yed_direct_draw(frame->bheight + frame->btop - 1,
                         frame->bwidth + frame->bleft - 1,
                         yed_active_style_get_active(),
                         "⇲");
}

void mouse_unload(yed_plugin *self) {
}
