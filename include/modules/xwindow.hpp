#pragma once

#include "modules/meta/event_handler.hpp"
#include "modules/meta/static_module.hpp"
#include "x11/ewmh.hpp"
#include "x11/icccm.hpp"
#include "x11/window.hpp"

POLYBAR_NS

class connection;

namespace modules {
  class active_window {
   public:
    explicit active_window(xcb_connection_t* conn, xcb_window_t win);
    ~active_window();

    bool match(const xcb_window_t win) const;
    string title() const;
    xcb_window_t get_window() const;

   private:
    xcb_connection_t* m_connection{nullptr};
    xcb_window_t m_window{XCB_NONE};
  };

  /**
   * Module used to display information about the
   * currently active X window.
   */
  class xwindow_module : public static_module<xwindow_module>, public event_handler<evt::property_notify> {
   public:
    explicit xwindow_module(const bar_settings&, string);

    void update(bool force = false);
    bool build(builder* builder, const string& tag) const;

   protected:
    void handle(const evt::property_notify& evt);

   private:
    static constexpr const char* TAG_LABEL{"<label>"};
    bool active_window_on_monitor(xcb_window_t window, monitor_t monitor) const;

    connection& m_connection;
    unique_ptr<active_window> m_active;
    label_t m_label;

    bool m_pinoutput{false};
  };
}

POLYBAR_NS_END
