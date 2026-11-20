#pragma once

using Mouse_Click_CB = void (*)(float x, float y, void *ctx);

bool install_mouse_click_monitor(void *mtk_view_handle,
    Mouse_Click_CB set_fg_pos_cb,
    void *ctx);

void remove_mouse_click_monitor();
