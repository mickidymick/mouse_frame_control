#include <yed/plugin.h>

static int                if_dragging_resize;
static int                if_dragging_move;
static yed_frame         *drag_frame;
static u64                last_timestamp;
static int                box_h, box_w;
static int                mouse_loc_r;
static int                mouse_loc_c;
static yed_direct_draw_t* dd;

static void       mouse(yed_event* event);
static void       mouse_unload(yed_plugin *self);
static yed_frame *find_frame(yed_event* event);
static int        yed_cell_is_in_frame_mouse(int row, int col, yed_frame *frame);
static int        is_in_bottom_right(yed_frame* frame, yed_event* event);
static void       middle_click(yed_event* event);
static void       middle_drag(yed_event* event);
static void       middle_release(yed_event* event);
static void       draw_corner(yed_event* event);
static void       draw(void);

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

    box_h              = 5;
    box_w              = 5;
    mouse_loc_r        = 0;
    mouse_loc_c        = 0;
    if_dragging_resize = 0;
    if_dragging_move   = 0;

    return 0;
}

int yed_cell_is_in_frame_mouse(int row, int col, yed_frame *frame) {
    return    (row >= frame->top  && row <= frame->top  + frame->height - 1)
           && (col >= frame->left && col <= frame->left + frame->width  - 1);
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
    if (frame == NULL) {return 0;}

    if (MOUSE_ROW(event->key) <= frame->bheight + frame->top         &&
        MOUSE_ROW(event->key) >= frame->bheight + frame->top - box_h &&
        MOUSE_COL(event->key) <= frame->bwidth + frame->left         &&
        MOUSE_COL(event->key) >= frame->bwidth + frame->left - box_w) {
            return 1;
    }else{
        return 0;
    }

}

static void middle_click(yed_event* event) {
    yed_frame *frame;

    frame = find_frame(event);
    if (frame == NULL) {
        return;
    }
    yed_activate_frame(frame);

    mouse_loc_r = MOUSE_ROW(event->key);
    mouse_loc_c = MOUSE_COL(event->key);

    event->cancel = 1;
}

static void middle_drag(yed_event* event) {
    yed_frame *frame;
    int        save;
    float      unit_x, unit_y;
    float      start  = 0.20;
    float      width  = 0.60;


    frame = ys->active_frame;
    if (!yed_frame_is_tree_root(frame)) {return;}

    if (if_dragging_resize || is_in_bottom_right(frame, event)) {
/*         resize frame */
        if (if_dragging_resize == 0) {
            if_dragging_resize = 1;
            mouse_loc_r = MOUSE_ROW(event->key);
            mouse_loc_c = MOUSE_COL(event->key);
        }else {
/* code pulled from frame_move_take_key */
            unit_x = 1.0 / (float)ys->term_cols;
            unit_y = 1.0 / (float)ys->term_rows;

/*             update height */
            if (MOUSE_ROW(event->key) > mouse_loc_r) {
                for(int i=0; i<(MOUSE_ROW(event->key) - mouse_loc_r); i++) {
                    if ((frame->btop + 1) + frame->bheight < ys->term_rows) {
                        save = frame->bheight;
                        do {
                            frame->height_f += unit_y;
                            FRAME_RESET_RECT(frame);
                        } while (frame->bheight == save);
                    }
                }

            }else if (MOUSE_ROW(event->key) < mouse_loc_r) {
                for(int i=0; i<(mouse_loc_r - MOUSE_ROW(event->key)); i++) {
                    if (frame->height > 1) {
                        save = frame->bheight;
                        do {
                            frame->height_f -= unit_y;
                            FRAME_RESET_RECT(frame);
                        } while (frame->bheight == save);
                    }
                }
            }
            mouse_loc_r = MOUSE_ROW(event->key);
/*             update width */
            if (MOUSE_COL(event->key) > mouse_loc_c) {
                for(int i=0; i<(MOUSE_COL(event->key) - mouse_loc_c); i++) {
                    if ((frame->bleft + 1) + frame->bwidth - 1 < ys->term_cols + 1) {
                        save = frame->bwidth;
                        do {
                            frame->width_f += unit_x;
                            FRAME_RESET_RECT(frame);
                        } while (frame->bwidth == save);
                    }
                }
            }else if (MOUSE_COL(event->key) < mouse_loc_c) {
                for(int i=0; i<(mouse_loc_c - MOUSE_COL(event->key)); i++) {
                    if (frame->width > 1) {
                        save = frame->bwidth;
                        do {
                            frame->width_f -= unit_x;
                            FRAME_RESET_RECT(frame);
                        } while (frame->bwidth == save);
                    }
                }
            }
            mouse_loc_c = MOUSE_COL(event->key);

        }
    }else{
/*         move frame */
        if (if_dragging_move == 0) {
            if_dragging_move = 1;
            mouse_loc_r = MOUSE_ROW(event->key);
            mouse_loc_c = MOUSE_COL(event->key);
        }else {
/* code pulled from frame_move_take_key */
            unit_x = 1.0 / (float)ys->term_cols;
            unit_y = 1.0 / (float)ys->term_rows;

/*             update height */
/*             MOVE DOWN */
            if (MOUSE_ROW(event->key) > mouse_loc_r) {
                if (frame->btop == 1 && frame->bwidth == ys->term_cols) {
                    frame->height_f = width;
                    frame->left_f   = start;
                    frame->width_f  = width;
                }
                for(int i=0; i<(MOUSE_ROW(event->key) - mouse_loc_r); i++) {
                    if ((frame->btop + 1) + frame->bheight < ys->term_rows) {
                        save = frame->btop;
                        do {
                            frame->top_f += unit_y;
                            FRAME_RESET_RECT(frame);
                        } while (frame->btop == save);
                    }
                }
/*             MOVE UP */
            }else if (MOUSE_ROW(event->key) < mouse_loc_r) {
                for(int i=0; i<(mouse_loc_r - MOUSE_ROW(event->key)); i++) {
                    if (frame->btop > 1) {
                        save = frame->btop;
                        do {
                            frame->top_f -= unit_y;
                            FRAME_RESET_RECT(frame);
                        } while (frame->btop == save);
                    }
                }
                if (MOUSE_ROW(event->key) == 1) {
                    frame->top_f    = 0;
                    frame->height_f = 1.0;
                    frame->left_f   = 0;
                    frame->width_f  = 1.0;
                    FRAME_RESET_RECT(frame);
                }
            }
            mouse_loc_r = MOUSE_ROW(event->key);

/*             update width */
/*             MOVE RIGHT */
            if (MOUSE_COL(event->key) > mouse_loc_c) {
                if ((frame->bwidth != ys->term_cols || frame->btop != 1) &&
                    frame->bleft == 1 &&
                    frame->bheight == ys->term_rows - 2) {

                    frame->top_f    = start;
                    frame->height_f = width;
                    frame->width_f  = width;
                }
                for(int i=0; i<(MOUSE_COL(event->key) - mouse_loc_c); i++) {
                    if ((frame->bleft + 1) + frame->bwidth - 1 < ys->term_cols + 1) {
                        save = frame->bleft;
                        do {
                            frame->left_f += unit_x;
                            FRAME_RESET_RECT(frame);
                        } while (frame->bleft == save);
                    }
                }

                if (MOUSE_COL(event->key) == ys->term_cols) {
                    frame->height_f = 1.0;
                    frame->top_f    = 0;
                    frame->left_f   = 0.5;
                    frame->width_f  = 0.5;
                    FRAME_RESET_RECT(frame);
                }
/*             MOVE LEFT */
            }else if (MOUSE_COL(event->key) < mouse_loc_c) {
                if ((frame->bwidth != ys->term_cols || frame->btop != 1) &&
                    frame->bleft +frame->bwidth - 1 == ys->term_cols &&
                    frame->bheight == ys->term_rows - 2) {

                    frame->top_f    = start;
                    frame->height_f = width;
                    frame->left_f   = 0.40;
                    frame->width_f  = width;
                }
                for(int i=0; i<(mouse_loc_c - MOUSE_COL(event->key)); i++) {
                    if (frame->bleft > 1) {
                        save = frame->bleft;
                        do {
                            frame->left_f -= unit_x;
                            FRAME_RESET_RECT(frame);
                        } while (frame->bleft == save);
                    }
                }

                if (MOUSE_COL(event->key) == 1) {
                    frame->top_f    = 0;
                    frame->height_f = 1.0;
                    frame->left_f   = 0;
                    frame->width_f  = 0.5;
                    FRAME_RESET_RECT(frame);
                }
            }
            mouse_loc_c = MOUSE_COL(event->key);
        }
    }
    draw();
    event->cancel = 1;
/*     yed_set_cursor_far_within_frame(frame, frame->cursor_line, frame->cursor_col); */
    yed_frame_hard_reset_cursor_x(frame);
    yed_frame_hard_reset_cursor_y(frame);
}

static void middle_release(yed_event* event) {
    event->cancel      = 1;
    if_dragging_resize = 0;
    if_dragging_move   = 0;
    mouse_loc_r        = 0;
    mouse_loc_c        = 0;
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
