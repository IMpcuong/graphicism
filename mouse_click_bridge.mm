#import <AppKit/AppKit.h>
#import <MetalKit/MetalKit.h>

#include "mouse_click_bridge.h"

static id g_mouse_monitor = nil;

bool install_mouse_click_monitor(void *view_handle,
    Mouse_Click_CB set_fg_pos_cb,
    void *ctx)
{
  MTKView *view = (__bridge MTKView *)view_handle;

  NSEvent * (^click_handler)(NSEvent *) =
    ^NSEvent * (NSEvent *event)
    {
      if (event.window == view.window)
      {
        NSPoint p = [view convertPoint:event.locationInWindow fromView:nil];
        NSRect bound = view.bounds;

        float x = 2.0f * float((p.x - NSMinX(bound)) / NSWidth(bound)) - 1.0f;
        float y = 2.0f * float((p.y - NSMinY(bound)) / NSHeight(bound)) - 1.0f;

        set_fg_pos_cb(x, y, ctx);
        NSLog(@"x: %f, y: %f", x, y);
      }

      return event;
    };

  g_mouse_monitor = [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskLeftMouseDown handler:click_handler];
  return g_mouse_monitor != nil;
}

void remove_mouse_click_monitor()
{
  if (g_mouse_monitor)
  {
    [NSEvent removeMonitor:g_mouse_monitor];
    g_mouse_monitor = nil;
  }
}
