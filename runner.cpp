#include <gtk/gtk.h>
#include <gtk4-layer-shell.h>
#include <gio/gio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <algorithm>

GtkWidget* runner_window = nullptr;
GtkWidget* search_entry = nullptr;
GtkWidget* results_scroll = nullptr;
GtkWidget* results_revealer = nullptr;
GtkWidget* results_list = nullptr;
GtkCssProvider* g_css_provider = nullptr;

bool is_expanded = false;

// ===================== ТЕМА =====================
struct WalColors {
    std::string fg = "#ffffff"; 
    std::string accent = "#9929CF"; 
    std::string text_sub = "#888888";
};
static WalColors g_wal;

std::vector<GAppInfo*> all_apps;

// ===================== УТИЛІТИ =====================
static std::string exec(const std::string& cmd) {
    std::string result;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";
    char buf[128];
    while (fgets(buf, sizeof(buf), pipe)) result += buf;
    pclose(pipe);
    return result;
}

// ФІКС №1: Нормальний пошук для кирилиці (UTF-8)
std::string to_lower(const std::string& s) {
    char* lower = g_utf8_strdown(s.c_str(), -1);
    std::string res = lower ? lower : s;
    g_free(lower);
    return res;
}

std::string parse_css_var(const std::string& line, const std::string& var_name) {
    auto pos = line.find(var_name + ":");
    if (pos == std::string::npos) return "";
    auto start = line.find('#', pos);
    if (start == std::string::npos) return "";
    auto end = line.find(';', start);
    std::string val = line.substr(start, (end == std::string::npos ? line.size() : end) - start);
    while (!val.empty() && (val.back() == ' ' || val.back() == ';')) val.pop_back();
    return val;
}

void reload_wal_colors() {
    std::ifstream f(std::string(getenv("HOME")) + "/.cache/wal/colors.css");
    if (!f.is_open()) return;
    std::string line;
    while (std::getline(f, line)) {
        std::string v;
        if (!(v = parse_css_var(line, "--color5")).empty()) g_wal.accent = v;
        if (!(v = parse_css_var(line, "--color8")).empty()) g_wal.text_sub = v;
        if (!(v = parse_css_var(line, "--foreground")).empty()) g_wal.fg = v;
    }
}

void apply_css() {
    if (!g_css_provider) return;
    reload_wal_colors(); 

    std::string css = R"(
        window { background: transparent; }
        * { font-family: "JetBrainsMono Nerd Font", "SF Pro Display", sans-serif; outline: none; box-shadow: none; }
        
        scrolledwindow, viewport, flowbox, .results-list {
            background-color: transparent;
            background: none;
            border: none;
        }

        window .main-container .results-list flowboxchild {
            background-color: transparent;
            border: none;
            box-shadow: none;
            outline: none;
            padding: 8px;
            margin: 2px;
            border-radius: 16px;
            transition: all 0.2s ease;
        }
        window .main-container .results-list flowboxchild:selected,
        window .main-container .results-list flowboxchild:focus,
        window .main-container .results-list flowboxchild:hover {
            background-color: rgba(255, 255, 255, 0.2);
            border: none;
            box-shadow: none;
            outline: none;
        }
        
        .main-container {
            background-color: rgba(20, 20, 25, 0.5);
            border: 1px solid rgba(255, 255, 255, 0.1);
            border-radius: 36px;
            padding: 6px 12px;
            transition: all 0.2s ease-out;
        }

        .search-box {
            background-color: transparent;
            border: none;
            padding: 0px 4px;
        }

        .search-entry {
            background: transparent;
            color: #ffffff;
            font-size: 18px;
            font-weight: 500;
            border: none;
            box-shadow: none;
            padding: 2px 12px 0px 12px;
        }
        
        .search-entry:focus {
            background: transparent;
            box-shadow: none;
            border: none;
        }

        .expand-btn {
            background: rgba(255,255,255,0.1);
            color: #ffffff;
            font-size: 18px;
            border-radius: 50%;
            min-width: 36px;
            min-height: 36px;
            padding: 0;
            border: 1px solid rgba(255,255,255,0.3);
            transition: all 0.2s ease;
        }
        .expand-btn label {
            margin-top: 2px;
        }
        .expand-btn:hover { 
            background-color: rgba(255,255,255,0.3);
        }

        .app-row-box {
            padding: 10px;
            background-color: transparent;
            border: none;
        }

        .app-icon { 
            margin-bottom: 8px;
            -gtk-icon-shadow: 0 4px 8px rgba(0,0,0,0.2);
        }
        .app-name { 
            color: #ffffff; 
            font-size: 14px; 
            font-weight: 600; 
            text-shadow: 0 1px 3px rgba(0,0,0,0.4);
        }
        
        scrollbar slider { background-color: rgba(255,255,255,0.3); border-radius: 100px; min-width: 6px; }
    )";
    
    gtk_css_provider_load_from_string(g_css_provider, css.c_str());
}

// ===================== ЛОГІКА ПРОГРАМ =====================
void load_all_apps() {
    all_apps.clear();
    GList* apps = g_app_info_get_all();
    for (GList* l = apps; l != nullptr; l = l->next) {
        GAppInfo* app = G_APP_INFO(l->data);
        all_apps.push_back(app);
        g_object_ref(app);
    }
    g_list_free_full(apps, g_object_unref);
}

static void launch_app(GAppInfo* app) {
    const char* cmdline = g_app_info_get_commandline(app);
    std::string final_cmd;

    if (cmdline) {
        std::string cmd = cmdline;
        // Вирізаємо системні плейсхолдери (%U, %F і т.д.), які GTK зазвичай міняє сам
        std::vector<std::string> bad_args = {"%u", "%U", "%f", "%F", "%c", "%k"};
        for (const auto& arg : bad_args) {
            size_t pos;
            while ((pos = cmd.find(arg)) != std::string::npos) {
                cmd.replace(pos, arg.length(), "");
            }
        }
        final_cmd = cmd + " >/dev/null 2>&1 &";
    } else {
        // Якщо раптом cmdline пустий, беремо хоча б назву бінарника
        const char* exec_cmd = g_app_info_get_executable(app);
        if (exec_cmd) {
            final_cmd = std::string(exec_cmd) + " >/dev/null 2>&1 &";
        }
    }

    if (!final_cmd.empty()) {
        system(final_cmd.c_str());
    }

    gtk_widget_set_visible(runner_window, FALSE);
}

static void on_row_activated(GtkFlowBox* box, GtkFlowBoxChild* child, gpointer user_data) {
    if (!child) return;
    GAppInfo* app = G_APP_INFO(g_object_get_data(G_OBJECT(child), "app-info"));
    char* raw_cmd = (char*)g_object_get_data(G_OBJECT(child), "raw-cmd");

    if (app) {
        launch_app(app);
    } else if (raw_cmd) {
        std::string cmd = std::string(raw_cmd) + " >/dev/null 2>&1 &";
        system(cmd.c_str());
        gtk_widget_set_visible(runner_window, FALSE);
    }
}

void add_app_to_list(GAppInfo* app, bool is_grid) {
    GtkWidget* row = gtk_flow_box_child_new();
    gtk_widget_add_css_class(row, "app-row");
    
    GtkWidget* box;
    if (is_grid) {
        box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
        gtk_widget_set_margin_top(box, 12);
        gtk_widget_set_margin_bottom(box, 12);
        gtk_widget_set_margin_start(box, 8);
        gtk_widget_set_margin_end(box, 8);
        gtk_widget_set_valign(box, GTK_ALIGN_CENTER);
        gtk_widget_set_halign(box, GTK_ALIGN_CENTER);
    } else {
        box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
        gtk_widget_set_margin_top(box, 8);
        gtk_widget_set_margin_bottom(box, 8);
        gtk_widget_set_margin_start(box, 16);
        gtk_widget_set_margin_end(box, 16);
        gtk_widget_set_valign(box, GTK_ALIGN_CENTER);
        gtk_widget_set_halign(box, GTK_ALIGN_START);
    }

    GIcon* gicon = g_app_info_get_icon(app);
    GtkWidget* icon = nullptr;
    int pixel_size = is_grid ? 86 : 48;

    if (gicon) {
        icon = gtk_image_new_from_gicon(gicon);
        gtk_image_set_pixel_size(GTK_IMAGE(icon), pixel_size);
    } else {
        icon = gtk_image_new_from_icon_name("application-x-executable");
        gtk_image_set_pixel_size(GTK_IMAGE(icon), pixel_size);
    }

    const char* name = g_app_info_get_display_name(app);
    GtkWidget* name_lbl = gtk_label_new(name ? name : "Unknown");
    
    if (is_grid) {
        gtk_widget_set_halign(name_lbl, GTK_ALIGN_CENTER);
        gtk_label_set_max_width_chars(GTK_LABEL(name_lbl), 12);
    } else {
        gtk_widget_set_halign(name_lbl, GTK_ALIGN_START);
        gtk_label_set_max_width_chars(GTK_LABEL(name_lbl), 40);
    }
    
    gtk_label_set_ellipsize(GTK_LABEL(name_lbl), PANGO_ELLIPSIZE_END);
    gtk_widget_add_css_class(name_lbl, "app-name");

    gtk_box_append(GTK_BOX(box), icon);
    gtk_box_append(GTK_BOX(box), name_lbl);
    
    g_object_set_data(G_OBJECT(row), "app-info", app);
    gtk_flow_box_child_set_child(GTK_FLOW_BOX_CHILD(row), box);
    gtk_flow_box_append(GTK_FLOW_BOX(results_list), row);
}

void add_raw_cmd_to_list(const std::string& cmd) {
    GtkWidget* row = gtk_flow_box_child_new();
    gtk_widget_add_css_class(row, "app-row");
    
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    gtk_widget_set_margin_top(box, 8);
    gtk_widget_set_margin_bottom(box, 8);
    gtk_widget_set_margin_start(box, 16);
    gtk_widget_set_margin_end(box, 16);
    gtk_widget_set_valign(box, GTK_ALIGN_CENTER);
    gtk_widget_set_halign(box, GTK_ALIGN_START);

    GtkWidget* icon = gtk_image_new_from_icon_name("utilities-terminal");
    gtk_image_set_pixel_size(GTK_IMAGE(icon), 48);

    std::string run_text = "Run: " + cmd;
    GtkWidget* name_lbl = gtk_label_new(run_text.c_str());
    gtk_widget_set_halign(name_lbl, GTK_ALIGN_START);
    gtk_label_set_ellipsize(GTK_LABEL(name_lbl), PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars(GTK_LABEL(name_lbl), 40);
    gtk_widget_add_css_class(name_lbl, "app-name");

    gtk_box_append(GTK_BOX(box), icon);
    gtk_box_append(GTK_BOX(box), name_lbl);
    
    g_object_set_data_full(G_OBJECT(row), "raw-cmd", g_strdup(cmd.c_str()), g_free);
    gtk_flow_box_child_set_child(GTK_FLOW_BOX_CHILD(row), box);
    gtk_flow_box_append(GTK_FLOW_BOX(results_list), row);
}

std::vector<std::string> search_files(const std::string& query) {
    std::vector<std::string> results;
    if (query.length() < 3) return results;
    
    std::string cmd = "locate -i -l 5 '" + query + "' 2>/dev/null";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return results;
    
    char buffer[512];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        std::string line = buffer;
        if (!line.empty() && line.back() == '\n') line.pop_back();
        if (!line.empty()) results.push_back(line);
    }
    pclose(pipe);
    return results;
}

void add_file_to_list(const std::string& filepath) {
    GtkWidget* row = gtk_flow_box_child_new();
    gtk_widget_add_css_class(row, "app-row");
    
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    gtk_widget_set_margin_top(box, 8);
    gtk_widget_set_margin_bottom(box, 8);
    gtk_widget_set_margin_start(box, 16);
    gtk_widget_set_margin_end(box, 16);
    gtk_widget_set_valign(box, GTK_ALIGN_CENTER);
    gtk_widget_set_halign(box, GTK_ALIGN_START);

    GtkWidget* icon = gtk_image_new_from_icon_name("text-x-generic");
    gtk_image_set_pixel_size(GTK_IMAGE(icon), 48);

    size_t slash = filepath.find_last_of('/');
    std::string filename = (slash != std::string::npos) ? filepath.substr(slash + 1) : filepath;

    GtkWidget* name_lbl = gtk_label_new(filename.c_str());
    gtk_widget_set_halign(name_lbl, GTK_ALIGN_START);
    gtk_label_set_max_width_chars(GTK_LABEL(name_lbl), 40);
    gtk_label_set_ellipsize(GTK_LABEL(name_lbl), PANGO_ELLIPSIZE_END);
    gtk_widget_add_css_class(name_lbl, "app-name");

    gtk_box_append(GTK_BOX(box), icon);
    gtk_box_append(GTK_BOX(box), name_lbl);

    std::string cmd = "xdg-open '" + filepath + "'";
    g_object_set_data_full(G_OBJECT(row), "raw-cmd", g_strdup(cmd.c_str()), g_free);
    gtk_flow_box_child_set_child(GTK_FLOW_BOX_CHILD(row), box);
    gtk_flow_box_append(GTK_FLOW_BOX(results_list), row);
}

void update_results(const std::string& query) {
    GtkWidget* child = gtk_widget_get_first_child(results_list);
    while (child) {
        GtkWidget* next = gtk_widget_get_next_sibling(child);
        gtk_flow_box_remove(GTK_FLOW_BOX(results_list), child);
        child = next;
    }

    std::string lower_query = to_lower(query);
    int app_count = 0;
    bool is_grid = query.empty();

    if (is_grid) {
        gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(results_list), 6);
        gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(results_list), 6);
        gtk_widget_set_halign(results_list, GTK_ALIGN_CENTER);
    } else {
        gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(results_list), 1);
        gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(results_list), 1);
        gtk_widget_set_halign(results_list, GTK_ALIGN_FILL);
    }

    for (GAppInfo* app : all_apps) {
        const char* name = g_app_info_get_display_name(app);
        const char* exec_cmd = g_app_info_get_executable(app);
        if (!name) continue;
        
        std::string lower_name = to_lower(name);
        std::string lower_exec = exec_cmd ? to_lower(exec_cmd) : "";
        
        if (lower_query.empty() || lower_name.find(lower_query) != std::string::npos || lower_exec.find(lower_query) != std::string::npos) {
            add_app_to_list(app, is_grid);
            app_count++;
        }
    }
    
    if (!query.empty() && app_count < 15) {
        std::vector<std::string> files = search_files(query);
        for (const auto& file : files) {
            add_file_to_list(file);
            app_count++;
        }
    }

    if (app_count > 0) {
        if (!query.empty()) {
            add_raw_cmd_to_list(query);
        }
        gtk_revealer_set_reveal_child(GTK_REVEALER(results_revealer), TRUE);
    } else {
        gtk_revealer_set_reveal_child(GTK_REVEALER(results_revealer), FALSE);
    }
}

// ===================== ОБРОБНИКИ UI ТА КЛАВІАТУРИ =====================
static void on_search_changed(GtkEditable* editable, gpointer user_data) {
    const char* text = gtk_editable_get_text(editable);
    std::string query(text);
    
    if (query.length() > 0) {
        update_results(query);
        
        GtkFlowBoxChild* row = gtk_flow_box_get_child_at_index(GTK_FLOW_BOX(results_list), 0);
        if (row) gtk_flow_box_select_child(GTK_FLOW_BOX(results_list), row);
    } else {
        is_expanded = false;
        gtk_revealer_set_reveal_child(GTK_REVEALER(results_revealer), FALSE);
        
        GtkWidget* child = gtk_widget_get_first_child(results_list);
        while (child) {
            GtkWidget* next = gtk_widget_get_next_sibling(child);
            gtk_flow_box_remove(GTK_FLOW_BOX(results_list), child);
            child = next;
        }
    }
}

static void on_expand_clicked(GtkWidget* btn, gpointer user_data) {
    is_expanded = !is_expanded;
    if (is_expanded) {
        gtk_revealer_set_reveal_child(GTK_REVEALER(results_revealer), TRUE);
        update_results(""); 
    } else {
        const char* text = gtk_editable_get_text(GTK_EDITABLE(search_entry));
        if (strlen(text) == 0) {
            gtk_revealer_set_reveal_child(GTK_REVEALER(results_revealer), FALSE);
        } else {
            update_results(text);
        }
    }
    gtk_widget_grab_focus(search_entry);
}

static gboolean on_entry_key(GtkEventControllerKey* ctl, guint keyval, guint keycode, GdkModifierType state, gpointer user_data) {
    if (keyval == GDK_KEY_Escape) {
        gtk_widget_set_visible(runner_window, FALSE);
        return TRUE;
    }
    if (keyval == GDK_KEY_Tab) { 
        on_expand_clicked(nullptr, nullptr);
        return TRUE;
    }
    if (keyval == GDK_KEY_Down || keyval == GDK_KEY_Up || keyval == GDK_KEY_Left || keyval == GDK_KEY_Right) { 
        if (!gtk_revealer_get_reveal_child(GTK_REVEALER(results_revealer))) return FALSE;
        
        GList* selected = gtk_flow_box_get_selected_children(GTK_FLOW_BOX(results_list));
        int current_idx = -1;
        if (selected) {
            current_idx = gtk_flow_box_child_get_index(GTK_FLOW_BOX_CHILD(selected->data));
            g_list_free(selected);
        }
        
        int max_idx = -1;
        GtkWidget* child = gtk_widget_get_first_child(results_list);
        while (child) { max_idx++; child = gtk_widget_get_next_sibling(child); }
        if (max_idx < 0) return TRUE;
        
        int cols = (gtk_flow_box_get_min_children_per_line(GTK_FLOW_BOX(results_list)) > 1) ? 6 : 1;
        int new_idx = current_idx;
        
        if (current_idx == -1) {
            new_idx = 0;
        } else {
            if (keyval == GDK_KEY_Right) new_idx++;
            else if (keyval == GDK_KEY_Left) new_idx--;
            else if (keyval == GDK_KEY_Down) new_idx += cols;
            else if (keyval == GDK_KEY_Up) new_idx -= cols;
        }
        
        if (new_idx < 0) new_idx = 0;
        if (new_idx > max_idx) new_idx = max_idx;
        
        GtkFlowBoxChild* next_row = gtk_flow_box_get_child_at_index(GTK_FLOW_BOX(results_list), new_idx);
        if (next_row) {
            gtk_flow_box_select_child(GTK_FLOW_BOX(results_list), next_row);
            
            GtkAdjustment* adj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(results_scroll));
            double row_y, row_h;
            gtk_widget_translate_coordinates(GTK_WIDGET(next_row), results_list, 0, 0, nullptr, &row_y);
            row_h = gtk_widget_get_height(GTK_WIDGET(next_row));
            double val = gtk_adjustment_get_value(adj);
            double page = gtk_adjustment_get_page_size(adj);
            
            if (row_y < val) gtk_adjustment_set_value(adj, row_y);
            else if (row_y + row_h > val + page) gtk_adjustment_set_value(adj, row_y + row_h - page);
        }
        return TRUE;
    }
    if (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter) { 
        if (gtk_revealer_get_reveal_child(GTK_REVEALER(results_revealer))) {
            GList* selected = gtk_flow_box_get_selected_children(GTK_FLOW_BOX(results_list));
            GtkFlowBoxChild* row = nullptr;
            if (selected) {
                row = GTK_FLOW_BOX_CHILD(selected->data);
                g_list_free(selected);
            } else {
                row = gtk_flow_box_get_child_at_index(GTK_FLOW_BOX(results_list), 0);
            }
            
            if (row) {
                g_signal_emit_by_name(results_list, "child-activated", row);
            }
        } else {
            const char* text = gtk_editable_get_text(GTK_EDITABLE(search_entry));
            if (strlen(text) > 0) {
                std::string cmd = std::string(text) + " >/dev/null 2>&1 &";
                system(cmd.c_str());
                gtk_widget_set_visible(runner_window, FALSE);
            }
        }
        return TRUE;
    }
    return FALSE;
}

// ===================== SOCKET =====================
gboolean on_socket_message(GIOChannel *source, GIOCondition condition, gpointer data) {
    int server_fd = g_io_channel_unix_get_fd(source);
    int client_fd = accept(server_fd, NULL, NULL);
    
    if (client_fd >= 0) {
        char buffer[256] = {0};
        int bytes_read = read(client_fd, buffer, sizeof(buffer)-1);
        if (bytes_read > 0) {
            std::string command(buffer);
            if (command.find("show") != std::string::npos) {
                apply_css();
                gtk_editable_set_text(GTK_EDITABLE(search_entry), "");
                is_expanded = false;
                gtk_revealer_set_reveal_child(GTK_REVEALER(results_revealer), FALSE);
                
                gtk_widget_set_visible(runner_window, TRUE);
                gtk_window_present(GTK_WINDOW(runner_window)); 
                gtk_widget_grab_focus(search_entry);
            }
        }
        close(client_fd);
    }
    return G_SOURCE_CONTINUE;
}

void setup_ipc_socket(const char* socket_path) {
    unlink(socket_path); 
    int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd < 0) return;
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path)-1);
    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) return;
    listen(server_fd, 5);
    GIOChannel* channel = g_io_channel_unix_new(server_fd);
    g_io_add_watch(channel, G_IO_IN, on_socket_message, NULL);
}

// ===================== ACTIVATE =====================
static void activate(GtkApplication* app, gpointer) {
    static bool initialized = false;
    if (initialized) return;
    initialized = true;

    load_all_apps(); 
    setup_ipc_socket(std::string(getenv("HOME")).append("/.config/SYSui/runner.sock").c_str());

    g_css_provider = gtk_css_provider_new();
    apply_css();
    gtk_style_context_add_provider_for_display(gdk_display_get_default(), GTK_STYLE_PROVIDER(g_css_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    runner_window = gtk_application_window_new(app);
    gtk_layer_init_for_window(GTK_WINDOW(runner_window));
    gtk_layer_set_namespace(GTK_WINDOW(runner_window), "runner");
    gtk_layer_set_layer(GTK_WINDOW(runner_window), GTK_LAYER_SHELL_LAYER_OVERLAY);
    
    gtk_layer_set_anchor(GTK_WINDOW(runner_window), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
    gtk_layer_set_margin(GTK_WINDOW(runner_window), GTK_LAYER_SHELL_EDGE_TOP, 250); 
    
    GtkGesture* click = gtk_gesture_click_new();
    g_signal_connect(click, "pressed", G_CALLBACK(+[](GtkGestureClick* gesture, int n_press, double x, double y, gpointer data) {
        GtkWidget* picked = gtk_widget_pick(runner_window, x, y, GTK_PICK_DEFAULT);
        if (picked == runner_window) {
            gtk_widget_set_visible(runner_window, FALSE);
        }
    }), nullptr);
    gtk_widget_add_controller(runner_window, GTK_EVENT_CONTROLLER(click));
    
    gtk_layer_set_keyboard_mode(GTK_WINDOW(runner_window), GTK_LAYER_SHELL_KEYBOARD_MODE_EXCLUSIVE);

    GtkWidget* main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(main_box, "main-container");
    gtk_widget_set_size_request(main_box, 800, -1); 
    gtk_widget_set_halign(main_box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(main_box, GTK_ALIGN_START);

    GtkWidget* search_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_widget_add_css_class(search_box, "search-box");

    GtkWidget* expand_btn = gtk_button_new_with_label("󰀻"); 
    gtk_widget_add_css_class(expand_btn, "expand-btn");
    g_signal_connect(expand_btn, "clicked", G_CALLBACK(on_expand_clicked), nullptr);

    search_entry = gtk_entry_new();
    gtk_widget_set_hexpand(search_entry, TRUE);
    gtk_entry_set_placeholder_text(GTK_ENTRY(search_entry), "Шукати програму...");
    gtk_widget_add_css_class(search_entry, "search-entry");
    
    GtkEventController* entry_keys = gtk_event_controller_key_new();
    gtk_event_controller_set_propagation_phase(entry_keys, GTK_PHASE_CAPTURE);
    g_signal_connect(entry_keys, "key-pressed", G_CALLBACK(on_entry_key), nullptr);
    gtk_widget_add_controller(search_entry, entry_keys);
    
    g_signal_connect(search_entry, "changed", G_CALLBACK(on_search_changed), nullptr);

    gtk_box_append(GTK_BOX(search_box), expand_btn);
    gtk_box_append(GTK_BOX(search_box), search_entry);

    results_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(results_scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_max_content_height(GTK_SCROLLED_WINDOW(results_scroll), 450);
    gtk_scrolled_window_set_propagate_natural_height(GTK_SCROLLED_WINDOW(results_scroll), TRUE);
    gtk_widget_set_margin_top(results_scroll, 15);

    results_revealer = gtk_revealer_new();
    gtk_revealer_set_transition_type(GTK_REVEALER(results_revealer), GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN);
    gtk_revealer_set_transition_duration(GTK_REVEALER(results_revealer), 250);
    gtk_revealer_set_child(GTK_REVEALER(results_revealer), results_scroll);
    gtk_revealer_set_reveal_child(GTK_REVEALER(results_revealer), FALSE);

    results_list = gtk_flow_box_new();
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(results_list), 6);
    gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(results_list), 6);
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(results_list), GTK_SELECTION_SINGLE);
    gtk_widget_set_halign(results_list, GTK_ALIGN_CENTER);
    
    gtk_widget_add_css_class(results_list, "results-list");
    g_signal_connect(results_list, "child-activated", G_CALLBACK(on_row_activated), nullptr);
    
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(results_scroll), results_list);

    gtk_box_append(GTK_BOX(main_box), search_box);
    gtk_box_append(GTK_BOX(main_box), results_revealer);

    gtk_window_set_child(GTK_WINDOW(runner_window), main_box);

    GtkEventController* win_keys = gtk_event_controller_key_new();
    g_signal_connect(win_keys, "key-pressed", G_CALLBACK(+[](GtkEventControllerKey*, guint keyval, guint, GdkModifierType, gpointer) -> gboolean {
        if (keyval == GDK_KEY_Escape) {
            gtk_widget_set_visible(runner_window, FALSE);
            return TRUE;
        }
        return FALSE;
    }), nullptr);
    gtk_widget_add_controller(runner_window, win_keys);

    gtk_widget_set_visible(runner_window, FALSE);
}

int main(int argc, char** argv) {
    GtkApplication* app = gtk_application_new("com.megumin.runner", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), nullptr);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}