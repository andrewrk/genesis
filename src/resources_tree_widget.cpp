#include "resources_tree_widget.hpp"
#include "gui_window.hpp"
#include "gui.hpp"
#include "color.hpp"
#include "debug.hpp"
#include "spritesheet.hpp"
#include "settings_file.hpp"
#include "scroll_bar_widget.hpp"
#include "audio_graph.hpp"
#include "dragged_sample_file.hpp"
#include "menu_widget.hpp"

static void device_change_callback(Event, void *userdata) {
    ResourcesTreeWidget *resources_tree = (ResourcesTreeWidget *)userdata;
    resources_tree->refresh_devices();
    resources_tree->update_model();
}

static void scroll_change_callback(Event, void *userdata) {
    ResourcesTreeWidget *resources_tree = (ResourcesTreeWidget *)userdata;
    resources_tree->update_model();
}

static void add_to_project_handler(void *userdata) {
    ResourcesTreeWidget *resources_tree_widget = (ResourcesTreeWidget *)userdata;
    resources_tree_widget->add_clicked_sample_to_project();
}

static void audio_assets_change_callback(Event, void *userdata) {
    ResourcesTreeWidget *resources_tree_widget = (ResourcesTreeWidget *)userdata;
    resources_tree_widget->refresh_audio_assets();
    resources_tree_widget->update_model();
}

ResourcesTreeWidget::ResourcesTreeWidget(GuiWindow *gui_window,
        SettingsFile *settings_file, Project *the_project) :
    Widget(gui_window),
    context(gui_window->gui->_genesis_context),
    gui(gui_window->gui),
    text_color(color_fg_text()),
    selection_color(color_selection()),
    padding_top(4),
    padding_bottom(0),
    padding_left(4),
    padding_right(0),
    icon_spacing(4),
    icon_width(12),
    icon_height(12),
    item_padding_top(4),
    item_padding_bottom(4),
    settings_file(settings_file)
{
    display_node_count = 0;
    selected_node = nullptr;
    last_click_node = nullptr;
    project = the_project;

    scroll_bar = create<ScrollBarWidget>(gui_window, ScrollBarLayoutVert);
    scroll_bar->parent_widget = this;

    dummy_label = create<Label>(gui);

    gui->events.attach_handler(EventAudioDeviceChange, device_change_callback, this);
    gui->events.attach_handler(EventMidiDeviceChange, device_change_callback, this);
    scroll_bar->events.attach_handler(EventScrollValueChange, scroll_change_callback, this);
    project->events.attach_handler(EventProjectAudioAssetsChanged, audio_assets_change_callback, this);

    root_node = create_parent_node(nullptr, "");
    root_node->indent_level = -1;
    root_node->parent_data->expanded = true;

    playback_devices_root = create_parent_node(root_node, "Playback Devices");
    recording_devices_root = create_parent_node(root_node, "Recording Devices");
    midi_devices_root = create_parent_node(root_node, "MIDI Devices");
    audio_assets_root = create_parent_node(root_node, "Audio Assets");
    samples_root = create_parent_node(root_node, "External Samples");

    refresh_devices();
    refresh_audio_assets();
    scan_sample_dirs();

    sample_context_menu = create<MenuWidgetItem>(gui_window);
    MenuWidgetItem *add_to_project_menu = sample_context_menu->add_menu("&Add to Project", shortcut(VirtKeyEnter));
    add_to_project_menu->set_activate_handler(add_to_project_handler, this);
}

ResourcesTreeWidget::~ResourcesTreeWidget() {
    clear_display_nodes();

    gui->events.detach_handler(EventAudioDeviceChange, device_change_callback);
    gui->events.detach_handler(EventMidiDeviceChange, device_change_callback);

    destroy_node(root_node);

    destroy(dummy_label, 1);
    destroy(scroll_bar, 1);
}

void ResourcesTreeWidget::clear_display_nodes() {
    for (int i = 0; i < display_nodes.length(); i += 1) {
        NodeDisplay *node_display = display_nodes.at(i);
        destroy_node_display(node_display);
    }
    display_nodes.clear();
}

void ResourcesTreeWidget::draw(const glm::mat4 &projection) {
    bg.draw(gui_window, projection);
    scroll_bar->draw(projection);


    glEnable(GL_STENCIL_TEST);

    glStencilFunc(GL_ALWAYS, 1, 0xFF);
    glStencilOp(GL_KEEP, GL_REPLACE, GL_REPLACE);
    glStencilMask(0xFF);

    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glClear(GL_STENCIL_BUFFER_BIT);

    gui_window->fill_rect(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), projection * stencil_model);

    glStencilFunc(GL_EQUAL, 1, 0xFF);
    glStencilMask(0x00);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    for (int i = 0; i < display_node_count; i += 1) {
        NodeDisplay *node_display = display_nodes.at(i);
        if (node_display->node == selected_node) {
            gui_window->fill_rect(selection_color, projection * node_display->selected_model);
        }
        node_display->label->draw(projection * node_display->label_model, text_color);
        if (should_draw_icon(node_display->node)) {
            gui->draw_image_color(gui_window, node_display->node->icon_img,
                    projection * node_display->icon_model, text_color);
        }
    }

    glDisable(GL_STENCIL_TEST);
}

void ResourcesTreeWidget::refresh_devices() {
    int audio_device_count = genesis_get_audio_device_count(context);
    int midi_device_count = genesis_get_midi_device_count(context);
    int default_playback_index = genesis_get_default_playback_device_index(context);
    int default_recording_index = genesis_get_default_recording_device_index(context);
    int default_midi_index = genesis_get_default_midi_device_index(context);

    int record_i = 0;
    int playback_i = 0;
    for (int i = 0; i < audio_device_count; i += 1) {
        GenesisAudioDevice *audio_device = genesis_get_audio_device(context, i);
        bool playback = (genesis_audio_device_purpose(audio_device) == GenesisAudioDevicePurposePlayback);
        Node *node;
        if (playback) {
            if (playback_i < playback_devices_root->parent_data->children.length()) {
                node = playback_devices_root->parent_data->children.at(playback_i);
            } else {
                node = create_playback_node();
            }
        } else {
            if (record_i < recording_devices_root->parent_data->children.length()) {
                node = recording_devices_root->parent_data->children.at(record_i);
            } else {
                node = create_record_node();
            }
        }
        genesis_audio_device_unref(node->audio_device);
        node->audio_device = audio_device;
        String text = genesis_audio_device_description(audio_device);
        if (playback && i == default_playback_index) {
            text.append(" (default)");
        } else if (!playback && i == default_recording_index) {
            text.append(" (default)");
        }
        node->text = text;

        if (playback) {
            playback_i += 1;
        } else {
            record_i += 1;
        }
    }
    trim_extra_children(recording_devices_root, record_i);
    trim_extra_children(playback_devices_root, playback_i);

    int i;
    for (i = 0; i < midi_device_count; i += 1) {
        GenesisMidiDevice *midi_device = genesis_get_midi_device(context, i);
        Node *node;
        if (i < midi_devices_root->parent_data->children.length()) {
            node = midi_devices_root->parent_data->children.at(i);
            genesis_midi_device_unref(node->midi_device);
        } else {
            node = create_midi_node();
        }
        node->midi_device = midi_device;
        String text = genesis_midi_device_description(midi_device);
        if (i == default_midi_index)
            text.append(" (default)");
        node->text = text;
    }
    trim_extra_children(midi_devices_root, i);
}

ResourcesTreeWidget::NodeDisplay * ResourcesTreeWidget::create_node_display(Node *node) {
    NodeDisplay *result = create<NodeDisplay>();
    result->label = create<Label>(gui);
    result->node = node;
    node->display = result;
    ok_or_panic(display_nodes.append(result));
    return result;
}

void ResourcesTreeWidget::destroy_node_display(NodeDisplay *node_display) {
    if (node_display) {
        if (node_display->node)
            node_display->node->display = nullptr;
        destroy(node_display->label, 1);
        destroy(node_display, 1);
    }
}

void ResourcesTreeWidget::update_model() {

    int available_width = width - scroll_bar->width - padding_left - padding_right;
    int available_height = height - padding_bottom - padding_top;

    stencil_model = transform2d(padding_left, padding_top, available_width, available_height);

    bg.update(this, 0, 0, padding_left + available_width, height);

    // compute item positions
    int next_top = padding_top;
    update_model_stack.clear();
    ok_or_panic(update_model_stack.append(root_node));
    while (update_model_stack.length() > 0) {
        Node *child = update_model_stack.pop();
        add_children_to_stack(update_model_stack, child);
        if (child->indent_level == -1)
            continue;
        child->top = next_top;
        next_top += item_padding_top + dummy_label->height() + item_padding_bottom;
        child->bottom = next_top;
    }

    int full_height = next_top;

    scroll_bar->left = left + width - scroll_bar->min_width();
    scroll_bar->top = top;
    scroll_bar->width = scroll_bar->min_width();
    scroll_bar->height = height;
    scroll_bar->min_value = 0;
    scroll_bar->max_value = max(0, full_height - available_height);
    scroll_bar->set_handle_ratio(available_height, full_height);
    scroll_bar->set_value(scroll_bar->value);
    scroll_bar->on_resize();

    // now consider scroll position and create display nodes for nodes that
    // are visible
    display_node_count = 0;
    update_model_stack.clear();
    ok_or_panic(update_model_stack.append(root_node));
    while (update_model_stack.length() > 0) {
        Node *child = update_model_stack.pop();
        add_children_to_stack(update_model_stack, child);
        if (child->indent_level == -1)
            continue;

        if (child->bottom - scroll_bar->value >= padding_top &&
            child->top - scroll_bar->value < padding_top + available_height)
        {
            NodeDisplay *node_display;
            if (display_node_count >= display_nodes.length())
                node_display = create_node_display(child);
            else
                node_display = display_nodes.at(display_node_count);
            display_node_count += 1;

            node_display->node = child;
            child->display = node_display;

            node_display->top = child->top - scroll_bar->value;
            node_display->bottom = child->bottom - scroll_bar->value;

            node_display->icon_left = padding_left + (icon_width + icon_spacing) * child->indent_level;
            node_display->right = width - padding_right;

            int extra_indent = (child->icon_img != nullptr);
            int label_left = padding_left + (icon_width + icon_spacing) *
                (child->indent_level + extra_indent);
            int label_top = node_display->top + item_padding_top;
            node_display->label_model = transform2d(label_left, label_top);
            node_display->label->set_text(child->text);
            node_display->label->update();

            node_display->selected_model = transform2d(
                    node_display->icon_left, node_display->top,
                    node_display->right - node_display->icon_left,
                    node_display->bottom - node_display->top);


            if (child->icon_img) {
                node_display->icon_top = node_display->top +
                    (node_display->bottom - node_display->top) / 2 - icon_height / 2;
                float icon_scale_width = icon_width / (float)child->icon_img->width;
                float icon_scale_height = icon_height / (float)child->icon_img->height;
                node_display->icon_model = transform2d(
                        node_display->icon_left, node_display->icon_top,
                        icon_scale_width, icon_scale_height);
            }
        }
    }
}

void ResourcesTreeWidget::add_children_to_stack(List<Node *> &stack, Node *node) {
    if (node->node_type != NodeTypeParent)
        return;
    if (!node->parent_data->expanded)
        return;

    for (int i = node->parent_data->children.length() - 1; i >= 0; i -= 1) {
        Node *child = node->parent_data->children.at(i);
        child->indent_level = node->indent_level + 1;
        ok_or_panic(stack.append(child));
    }
}

ResourcesTreeWidget::Node *ResourcesTreeWidget::create_playback_node() {
    Node *node = ok_mem(create_zero<Node>());
    node->node_type = NodeTypePlaybackDevice;
    node->parent_node = playback_devices_root;
    node->icon_img = gui->img_volume_up;
    ok_or_panic(node->parent_node->parent_data->children.append(node));
    return node;
}

ResourcesTreeWidget::Node *ResourcesTreeWidget::create_record_node() {
    Node *node = ok_mem(create_zero<Node>());
    node->node_type = NodeTypeRecordingDevice;
    node->parent_node = recording_devices_root;
    node->icon_img = gui->img_microphone;
    ok_or_panic(node->parent_node->parent_data->children.append(node));
    return node;
}

ResourcesTreeWidget::Node *ResourcesTreeWidget::create_midi_node() {
    Node *node = ok_mem(create_zero<Node>());
    node->node_type = NodeTypeMidiDevice;
    node->parent_node = midi_devices_root;
    node->icon_img = gui->img_music;
    ok_or_panic(node->parent_node->parent_data->children.append(node));
    return node;
}

ResourcesTreeWidget::Node *ResourcesTreeWidget::create_audio_asset_node() {
    Node *node = ok_mem(create_zero<Node>());
    node->node_type = NodeTypeAudioAsset;
    node->parent_node = audio_assets_root;
    node->icon_img = gui->img_entry_file;
    ok_or_panic(node->parent_node->parent_data->children.append(node));
    return node;
}

ResourcesTreeWidget::Node *ResourcesTreeWidget::create_parent_node(Node *parent, const char *text) {
    Node *node = ok_mem(create_zero<Node>());
    node->text = text;
    node->icon_img = gui->img_plus;
    node->node_type = NodeTypeParent;
    node->parent_data = create<ParentNode>();
    node->parent_data->expanded = false;
    node->parent_node = parent;
    if (parent)
        ok_or_panic(parent->parent_data->children.append(node));
    return node;
}

ResourcesTreeWidget::Node *ResourcesTreeWidget::create_sample_file_node(Node *parent,
        OsDirEntry *dir_entry, const ByteBuffer &full_path)
{
    Node *node = ok_mem(create_zero<Node>());
    node->node_type = NodeTypeSampleFile;
    node->text = dir_entry->name;
    node->parent_node = parent;
    node->dir_entry = dir_entry;
    node->icon_img = gui->img_entry_file;
    node->full_path = full_path;
    ok_or_panic(node->parent_node->parent_data->children.append(node));
    return node;
}

void ResourcesTreeWidget::pop_destroy_child(Node *node) {
    Node *child = node->parent_data->children.pop();
    child->parent_node = nullptr;
    destroy_node(child);
}

void ResourcesTreeWidget::destroy_node(Node *node) {
    if (node) {
        if (node == selected_node)
            select_node(nullptr);
        if (node == last_click_node)
            last_click_node = nullptr;
        destroy(node->parent_data, 1);
        genesis_audio_device_unref(node->audio_device);
        genesis_midi_device_unref(node->midi_device);
        os_dir_entry_unref(node->dir_entry);
        destroy(node, 1);
    }
}

void ResourcesTreeWidget::on_mouse_wheel(const MouseWheelEvent *event) {
    float range = scroll_bar->max_value - scroll_bar->min_value;
    scroll_bar->set_value(scroll_bar->value - event->wheel_y * range * 0.18f * scroll_bar->handle_ratio);
    update_model();
}

void ResourcesTreeWidget::on_mouse_move(const MouseEvent *event) {
    if (forward_mouse_event(scroll_bar, event))
        return;

    if (event->action != MouseActionDown)
        return;

    for (int i = 0; i < display_nodes.length(); i += 1) {
        NodeDisplay *node_display = display_nodes.at(i);
        Node *node = node_display->node;
        if (node->node_type == NodeTypeParent &&
            event->x >= node_display->icon_left &&
            event->x < node_display->icon_left + icon_width + icon_spacing &&
            event->y >= node_display->top &&
            event->y < node_display->bottom)
        {
            toggle_expansion(node);
            break;
        } else if (event->x >= node_display->icon_left &&
                   event->x < node_display->right &&
                   event->y >= node_display->top &&
                   event->y < node_display->bottom)
        {
            if (event->button == MouseButtonRight) {
                right_click_node(node, event);
            } else if (event->dbl_click_count > 0 && node == last_click_node) {
                double_click_node(node);
            } else {
                mouse_down_node(node, event);
            }
            break;
        }
    }
}

void ResourcesTreeWidget::select_node(Node *node) {
    selected_node = node;
    if (selected_node) {
        if (selected_node->node_type == NodeTypeSampleFile) {
            project_play_sample_file(project, selected_node->full_path);
        } else if (selected_node->node_type == NodeTypeAudioAsset) {
            project_play_audio_asset(project, selected_node->audio_asset);
        }
    }
}

static void destruct_sample_file_drag(DragData *drag_data) {
    DraggedSampleFile *dragged_sample_file = (DraggedSampleFile *)drag_data->ptr;
    destroy(dragged_sample_file, 1);
}

void ResourcesTreeWidget::mouse_down_node(Node *node, const MouseEvent *event) {
    last_click_node = node;
    select_node(node);

    if (node->node_type == NodeTypeSampleFile) {
        DraggedSampleFile *dragged_sample_file = ok_mem(create_zero<DraggedSampleFile>());
        dragged_sample_file->full_path = node->full_path;

        DragData *drag_data = ok_mem(create_zero<DragData>());
        drag_data->drag_type = DragTypeSampleFile;
        drag_data->ptr = dragged_sample_file;
        drag_data->destruct = destruct_sample_file_drag;
        gui_window->start_drag(event, drag_data);
    }
}

static void on_context_menu_destroy(ContextMenuWidget *context_menu) {
    ResourcesTreeWidget *resources_tree_widget = (ResourcesTreeWidget*)context_menu->userdata;
    resources_tree_widget->last_click_node = nullptr;
}

void ResourcesTreeWidget::right_click_node(Node *node, const MouseEvent *event) {
    last_click_node = node;
    select_node(node);
    if (node->node_type == NodeTypeSampleFile) {
        ContextMenuWidget *context_menu = pop_context_menu(sample_context_menu, event->x, event->y, 1, 1);
        context_menu->userdata = this;
        context_menu->on_destroy = on_context_menu_destroy;
    }
}

void ResourcesTreeWidget::double_click_node(Node *node) {
    if (node->node_type == NodeTypeParent) {
        toggle_expansion(node);
    }
}

ResourcesTreeWidget::Node *ResourcesTreeWidget::get_first_node() {
    if (root_node->parent_data->children.length() == 0)
        return nullptr;
    else
        return root_node->parent_data->children.at(0);
}

ResourcesTreeWidget::Node *ResourcesTreeWidget::get_last_node() {
    Node *node = root_node;
    while (node->node_type == NodeTypeParent && is_node_expanded(node)) {
        if (node->parent_data->children.length() > 0)
            node = node->parent_data->children.last();
        else
            break;
    }
    return node;
}

int ResourcesTreeWidget::get_node_index(Node *target_node) {
    Node *parent = target_node->parent_node;
    assert(parent->node_type == NodeTypeParent);
    for (int i = 0; i < parent->parent_data->children.length(); i += 1) {
        Node *child = parent->parent_data->children.at(i);
        if (child == target_node)
            return i;
    }
    panic("node not found");
}

void ResourcesTreeWidget::nav_sel_x(int dir) {
    if (!selected_node) {
        select_node((dir > 0) ? get_first_node() : get_last_node());
        return;
    }
    if (selected_node->node_type == NodeTypeParent) {
        if (dir > 0) {
            if (is_node_expanded(selected_node)) {
                nav_sel_y(1);
            } else {
                toggle_expansion(selected_node);
            }
        } else {
            if (is_node_expanded(selected_node)) {
                toggle_expansion(selected_node);
            } else {
                if (selected_node->parent_node != root_node)
                    select_node(selected_node->parent_node);
            }
        }
        return;
    }
    if (dir < 0) {
        if (selected_node->parent_node != root_node) {
            select_node(selected_node->parent_node);
            toggle_expansion(selected_node);
        }
    }
}

void ResourcesTreeWidget::nav_sel_y(int dir) {
    if (!selected_node) {
        select_node((dir > 0) ? get_first_node() : get_last_node());
        return;
    }
    if (dir > 0) {
        if (selected_node->node_type == NodeTypeParent &&
            is_node_expanded(selected_node) &&
            selected_node->parent_data->children.length() > 0)
        {
            select_node(selected_node->parent_data->children.at(0));
            return;
        }
        Node *target_node = selected_node;
        while (target_node != root_node) {
            Node *parent = target_node->parent_node;
            int target_node_index = get_node_index(target_node);
            if (target_node_index >= parent->parent_data->children.length() - 1) {
                target_node = parent;
            } else {
                select_node(parent->parent_data->children.at(target_node_index + 1));
                return;
            }
        }
    } else {
        Node *parent = selected_node->parent_node;
        int selected_node_index = get_node_index(selected_node);
        if (selected_node_index > 0) {
            Node *upper_node = parent->parent_data->children.at(selected_node_index - 1);
            while (upper_node->node_type == NodeTypeParent && is_node_expanded(upper_node)) {
                if (upper_node->parent_data->children.length() > 0) {
                    upper_node = upper_node->parent_data->children.last();
                } else {
                    break;
                }
            }
            select_node(upper_node);
        } else if (parent != root_node) {
            select_node(parent);
        }
    }
}

bool ResourcesTreeWidget::on_key_event(const KeyEvent *event) {
    if (event->action != KeyActionDown)
        return false;

    switch (event->virt_key) {
        default: break;
        case VirtKeyDown:
            nav_sel_y(1);
            return true;
        case VirtKeyUp:
            nav_sel_y(-1);
            return true;
        case VirtKeyLeft:
            nav_sel_x(-1);
            return true;
        case VirtKeyRight:
            nav_sel_x(1);
            return true;
    }

    if (selected_node) {
        if (selected_node->node_type == NodeTypeSampleFile) {
            if (MenuWidget::dispatch_shortcut(sample_context_menu, event)) {
                return true;
            }
        }
    }

    return false;
}

bool ResourcesTreeWidget::is_node_expanded(Node *node) {
    assert(node->node_type == NodeTypeParent);
    return node->parent_data->expanded;
}

void ResourcesTreeWidget::toggle_expansion(Node *node) {
    node->parent_data->expanded = !node->parent_data->expanded;
    node->icon_img = node->parent_data->expanded ? gui->img_minus : gui->img_plus;
    update_model();
}

bool ResourcesTreeWidget::should_draw_icon(Node *node) {
    if (!node->icon_img)
        return false;
    if (node->node_type == NodeTypeParent && node->parent_data->children.length() == 0)
        return false;
    return true;
}

void ResourcesTreeWidget::delete_all_children(Node *root) {
    assert(root->node_type == NodeTypeParent);
    List<Node *> pending;
    ok_or_panic(pending.append(root));
    while (pending.length() > 0) {
        Node *node = pending.pop();
        if (node->node_type == NodeTypeParent) {
            for (int i = 0; i < node->parent_data->children.length(); i += 1) {
                ok_or_panic(pending.append(node->parent_data->children.at(i)));
            }
        }
        if (node != root)
            destroy_node(node);
    }
}

static int compare_is_dir_then_name(OsDirEntry *a, OsDirEntry *b) {
    if (a->is_dir && !b->is_dir) {
        return -1;
    } else if (b->is_dir && !a->is_dir) {
        return 1;
    } else {
        return ByteBuffer::compare(a->name, b->name);
    }
}

void ResourcesTreeWidget::scan_dir_recursive(const ByteBuffer &dir, Node *parent_node) {
    List<OsDirEntry *> entries;
    int err = os_readdir(dir.raw(), entries);
    if (err)
        fprintf(stderr, "Error reading %s: %s\n", dir.raw(), genesis_error_string(err));
    entries.sort<compare_is_dir_then_name>();
    for (int i = 0; i < entries.length(); i += 1) {
        OsDirEntry *dir_entry = entries.at(i);
        ByteBuffer full_path = os_path_join(dir, dir_entry->name);
        if (dir_entry->is_dir) {
            Node *child = create_parent_node(parent_node, dir_entry->name.raw());
            scan_dir_recursive(full_path, child);
            os_dir_entry_unref(dir_entry);
        } else {
            create_sample_file_node(parent_node, dir_entry, full_path);
        }
    }
}

void ResourcesTreeWidget::scan_sample_dirs() {
    List<ByteBuffer> dirs;
    ok_or_panic(dirs.append(os_get_samples_dir()));
    for (int i = 0; i < settings_file->sample_dirs.length(); i += 1) {
        ok_or_panic(dirs.append(settings_file->sample_dirs.at(i)));
    }

    delete_all_children(samples_root);

    for (int i = 0; i < dirs.length(); i += 1) {
        Node *parent_node = create_parent_node(samples_root, dirs.at(i).raw());
        scan_dir_recursive(dirs.at(i), parent_node);
    }
}

void ResourcesTreeWidget::add_clicked_sample_to_project() {
    assert(selected_node);
    assert(selected_node->node_type == NodeTypeSampleFile);

    AudioAsset *audio_asset;
    int err;
    if ((err = project_add_audio_asset(project, selected_node->full_path, &audio_asset))) {
        if (err != GenesisErrorAlreadyExists)
            ok_or_panic(err);
    }
}

void ResourcesTreeWidget::refresh_audio_assets() {
    int i;
    for (i = 0; i < project->audio_asset_list.length(); i += 1) {
        Node *node;
        if (i < audio_assets_root->parent_data->children.length()) {
            node = audio_assets_root->parent_data->children.at(i);
        } else {
            node = create_audio_asset_node();
        }
        node->audio_asset = project->audio_asset_list.at(i);
        node->text = node->audio_asset->path;
    }
    trim_extra_children(audio_assets_root, i);
}

void ResourcesTreeWidget::trim_extra_children(Node *parent, int desired_children_count) {
    while (desired_children_count < parent->parent_data->children.length()) {
        pop_destroy_child(parent);
    }
}
