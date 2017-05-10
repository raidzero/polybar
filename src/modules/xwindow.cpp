#include "modules/xwindow.hpp"
#include "drawtypes/label.hpp"
#include "utils/factory.hpp"
#include "x11/atoms.hpp"
#include "x11/connection.hpp"

#include "modules/meta/base.inl"

POLYBAR_NS

namespace modules {
  template class module<xwindow_module>;

  /**
   * Wrapper used to update the event mask of the
   * currently active to enable title tracking
   */
  active_window::active_window(xcb_connection_t* conn, xcb_window_t win) : m_connection(conn), m_window(win) {
    if (m_window != XCB_NONE) {
      const unsigned int mask{XCB_EVENT_MASK_PROPERTY_CHANGE};
      xcb_change_window_attributes(m_connection, m_window, XCB_CW_EVENT_MASK, &mask);
    }
  }

  /**
   * Deconstruct window object
   */
  active_window::~active_window() {
    if (m_window != XCB_NONE) {
      const unsigned int mask{XCB_EVENT_MASK_NO_EVENT};
      xcb_change_window_attributes(m_connection, m_window, XCB_CW_EVENT_MASK, &mask);
    }
  }

  /**
   * Check if current window matches passed value
   */
  bool active_window::match(const xcb_window_t win) const {
    return m_window == win;
  }

  /**
   * Get the title by returning the first non-empty value of:
   *  _NET_WM_NAME
   *  _NET_WM_VISIBLE_NAME
   */
  string active_window::title() const {
    string title;

    if (!(title = ewmh_util::get_wm_name(m_window)).empty()) {
      return title;
    } else if (!(title = ewmh_util::get_visible_name(m_window)).empty()) {
      return title;
    } else if (!(title = icccm_util::get_wm_name(m_connection, m_window)).empty()) {
      return title;
    } else {
      return "";
    }
  }

  xcb_window_t active_window::get_window() const {
    return m_window;
  }

  bool xwindow_module::active_window_on_monitor(xcb_window_t window, monitor_t monitor) const {
    xcb_get_geometry_cookie_t cookie = xcb_get_geometry(m_connection, window);
    xcb_get_geometry_reply_t *reply = xcb_get_geometry_reply(m_connection, cookie, nullptr);

    if (reply) {
      xcb_translate_coordinates_reply_t  *t = xcb_translate_coordinates_reply(m_connection,
        xcb_translate_coordinates(m_connection, window, m_connection.root(), reply->x, reply->y), nullptr);

      printf("win.x: %d, win.y: %d (%dx%d)\n", t->dst_x, t->dst_y, reply->width, reply->height);
      printf("monitor x: %d, y: %d\n", monitor->x, monitor->y);
    }

    return true;
  }

  /**
   * Construct module
   */
  xwindow_module::xwindow_module(const bar_settings& bar, string name_)
      : static_module<xwindow_module>(bar, move(name_)), m_connection(connection::make()) {
    // Initialize ewmh atoms
    if ((ewmh_util::initialize()) == nullptr) {
      throw module_error("Failed to initialize ewmh atoms");
    }

    // Check if the WM supports _NET_ACTIVE_WINDOW
    if (!ewmh_util::supports(_NET_ACTIVE_WINDOW)) {
      throw module_error("The WM does not list _NET_ACTIVE_WINDOW as a supported hint");
    }

    // get list of monitors
    m_monitors = randr_util::get_monitors(m_connection, m_connection.root(), false);

    // Add formats and elements
    m_formatter->add(DEFAULT_FORMAT, TAG_LABEL, {TAG_LABEL});

    if (m_formatter->has(TAG_LABEL)) {
      m_label = load_optional_label(m_conf, name(), TAG_LABEL, "%title%");
    }
  }

  /**
   * Handler for XCB_PROPERTY_NOTIFY events
   */
  void xwindow_module::handle(const evt::property_notify& evt) {
    if (evt->atom == _NET_ACTIVE_WINDOW) {
      update(true);
    } else if (evt->atom == _NET_CURRENT_DESKTOP) {
      update(true);
    } else if (evt->atom == _NET_WM_VISIBLE_NAME) {
      update();
    } else if (evt->atom == _NET_WM_NAME) {
      update();
    } else {
      return;
    }

    broadcast();
  }

  /**
   * Update the currently active window and query its title
   */
  void xwindow_module::update(bool force) {
    std::lock(m_buildlock, m_updatelock);
    std::lock_guard<std::mutex> guard_a(m_buildlock, std::adopt_lock);
    std::lock_guard<std::mutex> guard_b(m_updatelock, std::adopt_lock);

    xcb_window_t win;

    if (force) {
      m_active.reset();
    }

    if (!m_active && (win = ewmh_util::get_active_window()) != XCB_NONE) {
      m_active = make_unique<active_window>(m_connection, win);
    }

    if (m_label) {
      m_label->reset_tokens();

      for (auto&& monitor : m_monitors) {
        if (active_window_on_monitor(m_active->get_window(), monitor)) {
          printf("YES!\n");
        }
      }

      m_label->replace_token("%title%", m_active ? m_active->title() : "");
    }
  }

  /**
   * Output content as defined in the config
   */
  bool xwindow_module::build(builder* builder, const string& tag) const {
    if (tag == TAG_LABEL && m_label && m_label.get()) {
      builder->node(m_label);
      return true;
    }
    return false;
  }
}

POLYBAR_NS_END
