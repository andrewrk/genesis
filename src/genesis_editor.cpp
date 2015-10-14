#include "genesis_editor.hpp"
#include "list.hpp"
#include "gui_window.hpp"
#include "genesis.h"
#include "grid_layout_widget.hpp"
#include "menu_widget.hpp"
#include "resources_tree_widget.hpp"
#include "track_editor_widget.hpp"
#include "project.hpp"
#include "settings_file.hpp"
#include "resource_bundle.hpp"
#include "dockable_pane_widget.hpp"
#include "text_widget.hpp"
#include "tab_widget.hpp"
#include "mixer_widget.hpp"
#include "piano_roll_widget.hpp"
#include "audio_graph.hpp"
#include "project_props_widget.hpp"

static void exit_handler(void *userdata) {
    GenesisEditor *genesis_editor = (GenesisEditor *)userdata;
    genesis_editor->gui->_running = false;
}

static void new_window_handler(void *userdata) {
    GenesisEditor *genesis_editor = (GenesisEditor *)userdata;
    genesis_editor->create_editor_window();
}

static void close_window_handler(void *userdata) {
    EditorWindow *editor_window = (EditorWindow *)userdata;
    editor_window->genesis_editor->close_window(editor_window);
}

static void close_others_handler(void *userdata) {
    EditorWindow *editor_window = (EditorWindow *)userdata;
    editor_window->genesis_editor->close_others(editor_window);
}

static void open_wiki_handler(void *userdata) {
    os_open_in_browser("https://github.com/andrewrk/genesis/wiki");
}

static void report_bug_handler(void *userdata) {
    os_open_in_browser("https://github.com/andrewrk/genesis/issues");
}

static void undo_handler(void *userdata) {
    GenesisEditor *genesis_editor = (GenesisEditor *)userdata;
    genesis_editor->do_undo();
}

static void redo_handler(void *userdata) {
    GenesisEditor *genesis_editor = (GenesisEditor *)userdata;
    genesis_editor->do_redo();
}

static void on_undo_changed(Event, void *userdata) {
    GenesisEditor *genesis_editor = (GenesisEditor *)userdata;
    genesis_editor->refresh_menu_state();
}

static void on_playing_changed(Event, void *userdata) {
    GenesisEditor *genesis_editor = (GenesisEditor *)userdata;
    genesis_editor->refresh_menu_state();
}

static void on_sample_rate_changed(Event, void *userdata) {
    GenesisEditor *genesis_editor = (GenesisEditor *)userdata;
    audio_graph_change_sample_rate(genesis_editor->project, genesis_editor->project->sample_rate);
}

static void always_show_tabs_handler(void *userdata) {
    EditorWindow *editor_window = (EditorWindow *)userdata;
    GenesisEditor *genesis_editor = editor_window->genesis_editor;
    editor_window->always_show_tabs = !editor_window->always_show_tabs;
    genesis_editor->refresh_menu_state();
    editor_window->dock_area->set_auto_hide_tabs(!editor_window->always_show_tabs);
    genesis_editor->save_window_config();
}

static void on_flush_events(Event, void *userdata) {
    GenesisEditor *genesis_editor = (GenesisEditor *)userdata;

    // update FPS labels
    ByteBuffer fps_text = ByteBuffer::format("%.0f fps", genesis_editor->gui->fps);
    for (int i = 0; i < genesis_editor->windows.length(); i += 1) {
        EditorWindow *editor_window = genesis_editor->windows.at(i);
        editor_window->fps_widget->set_text(fps_text);
    }

    project_flush_events(genesis_editor->project);
}

static void on_buffer_underrun(Event, void *userdata) {
    GenesisEditor *genesis_editor = (GenesisEditor *)userdata;

    // TODO tell the difference between buffer underruns and other types of errors
    double latency = genesis_get_latency(genesis_editor->genesis_context);
    double new_latency = latency + 0.005;
    fprintf(stderr, "recovering from stream error. latency %f -> %f\n", latency, new_latency);

    project_recover_stream(genesis_editor->project, new_latency);
}

static void on_sound_backend_disconnected(Event, void *userdata) {
    GenesisEditor *genesis_editor = (GenesisEditor *)userdata;

    project_recover_sound_backend_disconnect(genesis_editor->project);
}

static void show_dock_handler(void *userdata) {
    EditorPane *editor_pane = (EditorPane *)userdata;
    EditorWindow *editor_window = editor_pane->editor_window;
    GenesisEditor *genesis_editor = editor_window->genesis_editor;
    genesis_editor->show_view(editor_pane);
}

static void toggle_playback_handler(void *userdata) {
    GenesisEditor *genesis_editor = (GenesisEditor *)userdata;
    genesis_editor->toggle_playback();
}

static void restart_playback_handler(void *userdata) {
    GenesisEditor *genesis_editor = (GenesisEditor *)userdata;
    genesis_editor->restart_playback();
}

static void stop_playback_handler(void *userdata) {
    GenesisEditor *genesis_editor = (GenesisEditor *)userdata;
    genesis_editor->stop_playback();
}

GenesisEditor::GenesisEditor() :
    project(nullptr)
{
    underrun_count = 0;

    int err;

    ByteBuffer config_dir = os_get_app_config_dir();
    if ((err = os_mkdirp(config_dir)))
        panic("unable to make genesis path: %s", genesis_strerror(err));

    ByteBuffer samples_dir = os_get_samples_dir();
    if ((err = os_mkdirp(samples_dir)))
        panic("unable to make samples path: %s", genesis_strerror(err));

    ByteBuffer config_path = os_get_app_config_path();
    settings_file = settings_file_open(config_path);

    resource_bundle = create<ResourceBundle>("resources.bundle");

    if ((err = genesis_create_context(&genesis_context)))
        panic("unable to create genesis context: %s", genesis_strerror(err));

    gui = create<Gui>(genesis_context, resource_bundle);

    gui->events.attach_handler(EventFlushEvents, on_flush_events, this);
    gui->events.attach_handler(EventBufferUnderrun, on_buffer_underrun, this);
    gui->events.attach_handler(EventSoundBackendDisconnected, on_sound_backend_disconnected, this);
    gui->events.attach_handler(EventDeviceDesignationChange, on_sound_backend_disconnected, this);

    bool settings_dirty = false;
    if (settings_file->user_name.length() == 0) {
        settings_file->user_name = os_get_user_name();
        settings_dirty = true;
    }
    if (settings_file->user_id == uint256::zero()) {
        settings_file->user_id = uint256::random();
        settings_dirty = true;
    }
    if (settings_file->latency == 0.0) {
        settings_file->latency = 0.010; // 10 ms
        settings_dirty = true;
    }

    genesis_set_latency(genesis_context, settings_file->latency);
    user = user_create(settings_file->user_id, settings_file->user_name);

    bool create_new = true;
    if (settings_file->open_project_id != uint256::zero()) {
        ByteBuffer proj_dir = os_path_join(os_get_projects_dir(), settings_file->open_project_id.to_string());
        ByteBuffer proj_path = os_path_join(proj_dir, "project.gdaw");
        int err = project_open(proj_path.raw(), genesis_context, settings_file, user, &project);
        if (err) {
            fprintf(stderr, "Unable to load project: %s\n", genesis_strerror(err));
        } else {
            create_new = false;
        }
    }

    if (create_new) {
        uint256 id = uint256::random();
        ByteBuffer proj_dir = os_path_join(os_get_projects_dir(), id.to_string());
        ByteBuffer proj_path = os_path_join(proj_dir, "project.gdaw");
        ok_or_panic(os_mkdirp(proj_dir));
        ok_or_panic(project_create(proj_path.raw(), genesis_context, settings_file, id, user, &project));

        settings_file->open_project_id = id;
        settings_dirty = true;
    }

    genesis_set_sample_rate(genesis_context, project->sample_rate);

    project_set_up_audio_graph(project);

    if (settings_file->open_windows.length() == 0) {
        create_sf_open_window();
        settings_dirty = true;
    }
    while (settings_file->perspectives.length() < settings_file->open_windows.length()) {
        ok_or_panic(settings_file->perspectives.add_one());
        SettingsFilePerspective *perspective = &settings_file->perspectives.last();
        perspective->name = "Default";
        perspective->dock.split_ratio = 0.30f;
        perspective->dock.dock_type = SettingsFileDockTypeHoriz;
        perspective->dock.child_a = ok_mem(create_zero<SettingsFileDock>());
        perspective->dock.child_b = ok_mem(create_zero<SettingsFileDock>());

        perspective->dock.child_a->dock_type = SettingsFileDockTypeTabs;
        ok_or_panic(perspective->dock.child_a->tabs.append("Resources"));

        perspective->dock.child_b->dock_type = SettingsFileDockTypeTabs;
        ok_or_panic(perspective->dock.child_b->tabs.append("Track Editor"));
        ok_or_panic(perspective->dock.child_b->tabs.append("Mixer"));

        settings_dirty = true;
    }
    for (int i = 0; i < settings_file->open_windows.length(); i += 1) {
        SettingsFileOpenWindow *sf_open_window = &settings_file->open_windows.at(i);
        create_window(sf_open_window);
    }

    if (settings_dirty)
        settings_file_commit(settings_file);

    project->events.attach_handler(EventProjectUndoChanged, on_undo_changed, this);
    project->events.attach_handler(EventProjectPlayingChanged, on_playing_changed, this);
    project->events.attach_handler(EventProjectSampleRateChanged, on_sample_rate_changed, this);
}

GenesisEditor::~GenesisEditor() {
    project_close(project);
    user_destroy(user);
    genesis_destroy_context(genesis_context);
}

void GenesisEditor::create_editor_window() {
    create_window(create_sf_open_window());
    settings_file_commit(settings_file);
}

int GenesisEditor::window_index(EditorWindow *window) {
    for (int i = 0; i < windows.length(); i += 1) {
        EditorWindow *editor_window = windows.at(i);
        if (window == editor_window)
            return i;
    }
    panic("window not found");
}

void GenesisEditor::close_window(EditorWindow *editor_window) {
    int index = window_index(editor_window);
    bool last_one = (windows.length() == 1);
    windows.swap_remove(index);
    gui->destroy_window(editor_window->window);
    if (!last_one)
        save_window_config();
}

void GenesisEditor::close_others(EditorWindow *window) {
    while (windows.length() > 1) {
        EditorWindow *target_window = windows.at(windows.length() - 1);
        if (target_window == window)
            target_window = windows.at(windows.length() - 2);
        close_window(target_window);
    }
    save_window_config();
}

static void static_on_close_event(Event, void *userdata) {
    EditorWindow *editor_window = (EditorWindow *)userdata;
    editor_window->genesis_editor->close_window(editor_window);
}

static void save_window_config_handler(Event, void *userdata) {
    EditorWindow *editor_window = (EditorWindow *)userdata;
    editor_window->genesis_editor->save_window_config();
}

SettingsFileOpenWindow *GenesisEditor::create_sf_open_window() {
    ok_or_panic(settings_file->open_windows.add_one());
    SettingsFileOpenWindow *sf_open_window = &settings_file->open_windows.last();
    sf_open_window->perspective_index = 0;
    sf_open_window->left = 0;
    sf_open_window->top = 0;
    sf_open_window->width = 1366;
    sf_open_window->height = 768;
    sf_open_window->maximized = false;
    sf_open_window->always_show_tabs = true;
    return sf_open_window;
}

void GenesisEditor::create_window(SettingsFileOpenWindow *sf_open_window) {
    EditorWindow *editor_window = create<EditorWindow>();

    GuiWindow *new_window = gui->create_window(
            sf_open_window->left, sf_open_window->top,
            sf_open_window->width, sf_open_window->height);
    new_window->_userdata = editor_window;
    new_window->events.attach_handler(EventWindowClose, static_on_close_event, editor_window);
    new_window->events.attach_handler(EventWindowPosChange, save_window_config_handler, editor_window);
    new_window->events.attach_handler(EventWindowSizeChange, save_window_config_handler, editor_window);
    new_window->events.attach_handler(EventPerspectiveChange, save_window_config_handler, editor_window);

    if (sf_open_window->maximized)
        new_window->maximize();

    editor_window->always_show_tabs = sf_open_window->always_show_tabs;


    editor_window->genesis_editor = this;
    editor_window->window = new_window;

    ok_or_panic(windows.append(editor_window));

    editor_window->menu_widget = create<MenuWidget>(new_window);
    MenuWidgetItem *project_menu = editor_window->menu_widget->add_menu("&Project");
    MenuWidgetItem *edit_menu = editor_window->menu_widget->add_menu("&Edit");
    MenuWidgetItem *playback_menu = editor_window->menu_widget->add_menu("P&layback");
    MenuWidgetItem *window_menu = editor_window->menu_widget->add_menu("&Window");
    MenuWidgetItem *help_menu = editor_window->menu_widget->add_menu("&Help");

    MenuWidgetItem *exit_menu = project_menu->add_menu("E&xit", ctrl_shortcut(VirtKeyQ));

    MenuWidgetItem *undo_menu = edit_menu->add_menu("&Undo", ctrl_shortcut(VirtKeyZ));
    MenuWidgetItem *redo_menu = edit_menu->add_menu("&Redo", ctrl_shift_shortcut(VirtKeyZ));

    MenuWidgetItem *toggle_playback_menu = playback_menu->add_menu("&Play", shortcut(VirtKeySpace));
    MenuWidgetItem *restart_playback_menu = playback_menu->add_menu("&Restart", shortcut(VirtKeyEnter));
    MenuWidgetItem *stop_playback_menu = playback_menu->add_menu("&Stop", shift_shortcut(VirtKeyEnter));

    MenuWidgetItem *new_window_menu = window_menu->add_menu("&New Window", no_shortcut());
    MenuWidgetItem *close_window_menu = window_menu->add_menu("&Close", alt_shortcut(VirtKeyF4));
    MenuWidgetItem *close_others_menu = window_menu->add_menu("Close &Others", no_shortcut());
    MenuWidgetItem *always_show_tabs_menu = window_menu->add_menu("Always Show &Tabs", no_shortcut());
    MenuWidgetItem *show_view_menu = window_menu->add_menu("Show &View", no_shortcut());

    MenuWidgetItem *open_wiki_menu = help_menu->add_menu("Genesis &Wiki", shortcut(VirtKeyF1));
    MenuWidgetItem *report_bug_menu = help_menu->add_menu("&Report a Bug", no_shortcut());

    exit_menu->set_activate_handler(exit_handler, this);

    undo_menu->set_activate_handler(undo_handler, this);
    redo_menu->set_activate_handler(redo_handler, this);

    toggle_playback_menu->set_activate_handler(toggle_playback_handler, this);
    restart_playback_menu->set_activate_handler(restart_playback_handler, this);
    stop_playback_menu->set_activate_handler(stop_playback_handler, this);

    new_window_menu->set_activate_handler(new_window_handler, this);
    close_window_menu->set_activate_handler(close_window_handler, editor_window);
    close_others_menu->set_activate_handler(close_others_handler, editor_window);
    always_show_tabs_menu->set_activate_handler(always_show_tabs_handler, editor_window);

    open_wiki_menu->set_activate_handler(open_wiki_handler, this);
    report_bug_menu->set_activate_handler(report_bug_handler, this);

    editor_window->undo_menu = undo_menu;
    editor_window->redo_menu = redo_menu;
    editor_window->always_show_tabs_menu = always_show_tabs_menu;
    editor_window->show_view_menu = show_view_menu;
    editor_window->toggle_playback_menu = toggle_playback_menu;
    editor_window->restart_playback_menu = restart_playback_menu;
    editor_window->stop_playback_menu = stop_playback_menu;

    TextWidget *fps_widget = create<TextWidget>(new_window);
    fps_widget->set_text_interaction(false);
    fps_widget->set_background_color(editor_window->menu_widget->bg_color);
    fps_widget->set_min_width(50);
    fps_widget->set_max_width(50);
    editor_window->fps_widget = fps_widget;

    GridLayoutWidget *top_bar_grid_layout = create<GridLayoutWidget>(new_window);
    top_bar_grid_layout->padding = 0;
    top_bar_grid_layout->spacing = 0;
    top_bar_grid_layout->add_widget(editor_window->menu_widget, 0, 0, HAlignLeft, VAlignTop);
    top_bar_grid_layout->add_widget(fps_widget, 0, 1, HAlignRight, VAlignTop);

    ResourcesTreeWidget *resources_tree = create<ResourcesTreeWidget>(new_window, settings_file, project);
    add_dock(editor_window, resources_tree, "Resources");

    TrackEditorWidget *track_editor = create<TrackEditorWidget>(new_window, project);
    add_dock(editor_window, track_editor, "Track Editor");

    MixerWidget *mixer = create<MixerWidget>(new_window, project);
    add_dock(editor_window, mixer, "Mixer");

    PianoRollWidget *piano_roll = create<PianoRollWidget>(new_window, project);
    add_dock(editor_window, piano_roll, "Piano Roll");

    ProjectPropsWidget *project_props = create<ProjectPropsWidget>(new_window, project);
    add_dock(editor_window, project_props, "Project");

    DockAreaWidget *dock_area = create<DockAreaWidget>(new_window);
    editor_window->dock_area = dock_area;

    GridLayoutWidget *main_grid_layout = create<GridLayoutWidget>(new_window);
    main_grid_layout->padding = 0;
    main_grid_layout->spacing = 0;
    main_grid_layout->add_widget(top_bar_grid_layout, 0, 0, HAlignLeft, VAlignTop);
    main_grid_layout->add_widget(dock_area, 1, 0, HAlignLeft, VAlignTop);
    new_window->set_main_widget(main_grid_layout);

    SettingsFilePerspective *perspective = &settings_file->perspectives.at(sf_open_window->perspective_index);
    load_perspective(editor_window, perspective);
}

void GenesisEditor::add_dock(EditorWindow *editor_window, Widget *widget, const char *title) {
    DockablePaneWidget *pane = create<DockablePaneWidget>(widget, title);
    EditorPane *editor_pane = create<EditorPane>();

    editor_pane->pane = pane;
    editor_pane->editor_window = editor_window;
    editor_pane->show_menu_item = editor_window->show_view_menu->add_menu(pane->title, -1, no_shortcut());
    editor_pane->show_menu_item->set_activate_handler(show_dock_handler, editor_pane);

    ok_or_panic(editor_window->all_panes.append(editor_pane));
}

void GenesisEditor::exec() {
    gui->exec();
}

void GenesisEditor::refresh_menu_state() {
    bool undo_enabled = (project->undo_stack_index > 0);
    bool redo_enabled = (project->undo_stack_index < project->undo_stack.length());
    String undo_caption;
    String redo_caption;
    if (undo_enabled) {
        Command *cmd = project->undo_stack.at(project->undo_stack_index - 1);
        undo_caption = "&Undo ";
        undo_caption.append(cmd->description());
    } else {
        undo_caption = "&Undo";
    }
    if (redo_enabled) {
        Command *cmd = project->undo_stack.at(project->undo_stack_index);
        redo_caption = "&Redo ";
        redo_caption.append(cmd->description());
    } else {
        redo_caption = "&Redo";
    }

    bool is_playing = project_is_playing(project);

    for (int i = 0; i < windows.length(); i += 1) {
        EditorWindow *editor_window = windows.at(i);
        editor_window->undo_menu->set_enabled(undo_enabled);
        editor_window->undo_menu->set_caption(undo_caption);

        editor_window->redo_menu->set_enabled(redo_enabled);
        editor_window->redo_menu->set_caption(redo_caption);

        editor_window->always_show_tabs_menu->set_icon(
                editor_window->always_show_tabs ? gui->img_check : nullptr);

        editor_window->toggle_playback_menu->set_caption(is_playing ? "&Pause" : "&Play");

        editor_window->window->refresh_context_menu();
    }
}

void GenesisEditor::do_undo() {
    project_undo(project);
}

void GenesisEditor::do_redo() {
    project_redo(project);
}

void GenesisEditor::load_perspective(EditorWindow *editor_window, SettingsFilePerspective *perspective) {
    editor_window->dock_area->reset_state();

    load_dock(editor_window, editor_window->dock_area, &perspective->dock);


    editor_window->dock_area->set_auto_hide_tabs(!editor_window->always_show_tabs);
    refresh_menu_state();
    editor_window->window->main_widget->on_resize();
}

void GenesisEditor::load_dock(EditorWindow *editor_window, DockAreaWidget *dock_area, SettingsFileDock *sf_dock) {
    dock_area->layout = (DockAreaLayout)sf_dock->dock_type;
    switch (sf_dock->dock_type) {
        case SettingsFileDockTypeTabs:
            for (int i = 0; i < sf_dock->tabs.length(); i += 1) {
                String name = sf_dock->tabs.at(i);
                dock_area->add_tab_pane(get_pane_widget(editor_window, name));
            }
            break;
        case SettingsFileDockTypeHoriz:
        case SettingsFileDockTypeVert:
            assert(sf_dock->child_a);
            assert(sf_dock->child_b);
            dock_area->child_a = ok_mem(create_zero<DockAreaWidget>(editor_window->window));
            dock_area->child_b = ok_mem(create_zero<DockAreaWidget>(editor_window->window));
            dock_area->split_ratio = sf_dock->split_ratio;
            load_dock(editor_window, dock_area->child_a, sf_dock->child_a);
            load_dock(editor_window, dock_area->child_b, sf_dock->child_b);
            dock_area->child_a->parent_widget = dock_area;
            dock_area->child_b->parent_widget = dock_area;
            break;
    }
}

DockablePaneWidget *GenesisEditor::get_pane_widget(EditorWindow *editor_window, const String &title) {
    for (int i = 0; i < editor_window->all_panes.length(); i += 1) {
        EditorPane *editor_pane = editor_window->all_panes.at(i);
        DockablePaneWidget *pane = editor_pane->pane;
        if (String::compare(pane->title, title) == 0)
            return pane;
    }
    panic("pane not found: %s", title.encode().raw());
}

void GenesisEditor::save_window_config() {
    ok_or_panic(settings_file->open_windows.resize(windows.length()));
    ok_or_panic(settings_file->perspectives.resize(windows.length()));
    for (int i = 0; i < windows.length(); i += 1) {
        EditorWindow *editor_window = windows.at(i);
        GuiWindow *window = editor_window->window;
        SettingsFileOpenWindow *sf_open_window = &settings_file->open_windows.at(i);
        sf_open_window->perspective_index = i;
        sf_open_window->left = window->client_left;
        sf_open_window->top = window->client_top;
        sf_open_window->width = window->_client_width;
        sf_open_window->height = window->_client_height;
        sf_open_window->maximized = window->is_maximized;
        sf_open_window->always_show_tabs = editor_window->always_show_tabs;

        SettingsFilePerspective *perspective = &settings_file->perspectives.at(i);
        save_dock(editor_window->dock_area, &perspective->dock);
    }
    settings_file_commit(settings_file);
}

void GenesisEditor::save_dock(DockAreaWidget *dock_area, SettingsFileDock *sf_dock) {
    settings_file_clear_dock(sf_dock);
    sf_dock->dock_type = (SettingsFileDockType)dock_area->layout;
    switch (dock_area->layout) {
        case DockAreaLayoutTabs:
            if (dock_area->tab_widget) {
                for (int i = 0; i < dock_area->tab_widget->tabs.length(); i += 1) {
                    TabWidgetTab *tab = dock_area->tab_widget->tabs.at(i);
                    DockablePaneWidget *pane = (DockablePaneWidget *)tab->widget;
                    ok_or_panic(sf_dock->tabs.append(pane->title));
                }
            }
            break;
        case DockAreaLayoutHoriz:
        case DockAreaLayoutVert:
            sf_dock->split_ratio = dock_area->split_ratio;
            sf_dock->child_a = ok_mem(create_zero<SettingsFileDock>());
            sf_dock->child_b = ok_mem(create_zero<SettingsFileDock>());

            save_dock(dock_area->child_a, sf_dock->child_a);
            save_dock(dock_area->child_b, sf_dock->child_b);
            break;
    }
}

void GenesisEditor::show_view(EditorPane *editor_pane) {
    // find the pane. if we find it, switch tabs so that it is current and set focus to it
    // if we don't find it, add it to the main dock area
    EditorWindow *editor_window = editor_pane->editor_window;
    DockablePaneWidget *container_pane = find_pane(editor_pane, editor_window->dock_area);
    if (container_pane) {
        TabWidget *tab_widget = (TabWidget *) container_pane->parent_widget;
        tab_widget->select_widget(container_pane);
        editor_window->window->set_focus_widget(container_pane);
    } else {
        assert(!editor_pane->pane->parent_widget);
        editor_window->dock_area->add_left_pane(editor_pane->pane);
    }
}

DockablePaneWidget *GenesisEditor::find_pane(EditorPane *editor_pane, DockAreaWidget *dock_area) {
    switch (dock_area->layout) {
        case DockAreaLayoutTabs:
            if (dock_area->tab_widget) {
                for (int i = 0; i < dock_area->tab_widget->tabs.length(); i += 1) {
                    TabWidgetTab *tab = dock_area->tab_widget->tabs.at(i);
                    DockablePaneWidget *pane = (DockablePaneWidget *)tab->widget;
                    if (pane == editor_pane->pane)
                        return pane;
                }
            }
            return nullptr;
        case DockAreaLayoutHoriz:
        case DockAreaLayoutVert:
            DockablePaneWidget *pane = nullptr;
            if ((pane = find_pane(editor_pane, dock_area->child_a)))
                return pane;
            if ((pane = find_pane(editor_pane, dock_area->child_b)))
                return pane;
            return pane;
    }
    panic("invalid dock area layout");
}

void GenesisEditor::toggle_playback() {
    if (project_is_playing(project)) {
        project_pause(project);
    } else {
        project_play(project);
    }
}

void GenesisEditor::restart_playback() {
    project_restart_playback(project);
}

void GenesisEditor::stop_playback() {
    project_stop_playback(project);
}
