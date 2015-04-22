#include "resources_tree_widget.hpp"
#include "gui_window.hpp"
#include "gui.hpp"
#include "color.hpp"
#include "debug.hpp"
#include "spritesheet.hpp"

static void device_change_callback(void *userdata) {
    ResourcesTreeWidget *resources_tree = (ResourcesTreeWidget *)userdata;
    resources_tree->update_model();
}

ResourcesTreeWidget::ResourcesTreeWidget(GuiWindow *gui_window) :
    Widget(gui_window),
    context(gui_window->_gui->_genesis_context),
    gui(gui_window->_gui),
    bg_color(parse_color("#333333")),
    text_color(parse_color("#DCDCDC")),
    padding_top(4),
    padding_bottom(0),
    padding_left(4),
    padding_right(0),
    icon_spacing(4),
    icon_width(12),
    icon_height(12),
    item_padding_top(4),
    item_padding_bottom(4)
{
    gui->attach_audio_device_callback(device_change_callback, this);
    gui->attach_midi_device_callback(device_change_callback, this);

    root_node = create_parent_node(nullptr, "");
    root_node->indent_level = -1;
    root_node->parent_data->expanded = true;

    playback_devices_root = create_parent_node(nullptr, "Playback Devices");
    recording_devices_root = create_parent_node(nullptr, "Recording Devices");
    midi_devices_root = create_parent_node(nullptr, "MIDI Devices");

    ok_or_panic(root_node->parent_data->children.resize(3));
    root_node->parent_data->children.at(0) = playback_devices_root;
    root_node->parent_data->children.at(1) = recording_devices_root;
    root_node->parent_data->children.at(2) = midi_devices_root;
}

ResourcesTreeWidget::~ResourcesTreeWidget() {
    gui->detach_audio_device_callback(device_change_callback);
    gui->detach_midi_device_callback(device_change_callback);

    destroy_node(root_node);
}

void ResourcesTreeWidget::draw(const glm::mat4 &projection) {
    glm::mat4 bg_mvp = projection * bg_model;
    gui_window->fill_rect(bg_color, bg_mvp);

    draw_stack.clear();
    ok_or_panic(draw_stack.append(root_node));

    while (draw_stack.length() > 0) {
        Node *child = draw_stack.pop();
        add_children_to_stack(draw_stack, child);
        if (child->indent_level >= 0) {
            glm::mat4 label_mvp = projection * child->label_model;
            child->label->draw(gui_window, label_mvp, text_color);
            if (should_draw_icon(child)) {
                glm::mat4 icon_mvp = projection * child->icon_model;
                gui->draw_image_color(gui_window, child->icon_img, icon_mvp, text_color);
            }
        }
        switch (child->node_type) {
            case NodeTypeParent:
                break;
            case NodeTypeRecordingDevice:
                panic("TODO");
                break;
            case NodeTypePlaybackDevice:
                panic("TODO");
                break;
            case NodeTypeMidiDevice:
                panic("TODO");
                break;
        }
    }
}

void ResourcesTreeWidget::update_model() {
    bg_model = glm::scale(
                glm::translate(
                    glm::mat4(1.0f),
                    glm::vec3(left, top, 0.0f)),
                glm::vec3(width, height, 1.0f));

    /*
    int audio_device_count = genesis_get_audio_device_count(context);
    int midi_device_count = genesis_get_midi_device_count(context);
    */

    update_model_stack.clear();
    ok_or_panic(update_model_stack.append(root_node));

    int next_top = padding_top;

    while (update_model_stack.length() > 0) {
        Node *child = update_model_stack.pop();
        add_children_to_stack(update_model_stack, child);
        switch (child->node_type) {
            case NodeTypeParent:
                if (child->indent_level >= 0) {
                    child->label->update();
                    child->top = next_top;
                    next_top += item_padding_top + child->label->height() + item_padding_bottom;
                    child->bottom = next_top;

                    int extra_indent = (child->icon_img != nullptr);
                    int label_left = left + padding_left + (icon_width + icon_spacing) *
                        (child->indent_level + extra_indent);
                    int label_top = top + child->top + item_padding_top;
                    child->label_model = glm::translate(
                                    glm::mat4(1.0f),
                                    glm::vec3(label_left, label_top, 0.0f));
                    if (child->icon_img) {
                        child->icon_left = padding_left + (icon_width + icon_spacing) * child->indent_level;
                        child->icon_top = child->top + (child->bottom - child->top) / 2 - icon_height / 2;
                        float icon_scale_width = icon_width / (float)child->icon_img->width;
                        float icon_scale_height = icon_height / (float)child->icon_img->height;
                        child->icon_model = glm::scale(
                                        glm::translate(
                                            glm::mat4(1.0f),
                                            glm::vec3(left + child->icon_left, top + child->icon_top, 0.0f)),
                                        glm::vec3(icon_scale_width, icon_scale_height, 1.0f));
                    }
                }
                break;
            case NodeTypeRecordingDevice:
                panic("TODO");
                break;
            case NodeTypePlaybackDevice:
                panic("TODO");
                break;
            case NodeTypeMidiDevice:
                panic("TODO");
                break;
        }
    }
    draw_stack.ensure_capacity(update_model_stack.capacity());

    /*
    for (int i = 0; i < audio_device_count; i += 1) {

    }
    */
}

ResourcesTreeWidget::Node *ResourcesTreeWidget::create_parent_node(Node *parent, const char *text) {
    Node *node = create_zero<Node>();
    if (!node)
        panic("out of memory");
    node->label = create<Label>(gui);
    node->label->set_text(text);
    node->icon_img = gui->img_plus;
    node->node_type = NodeTypeParent;
    node->parent_data = create<ParentNode>();
    node->parent_data->expanded = false;
    node->parent_node = parent;
    return node;
}

void ResourcesTreeWidget::destroy_node(Node *node) {
    if (node) {
        destroy(node->label, 1);
        destroy(node->parent_data, 1);
        genesis_audio_device_unref(node->audio_device);
        genesis_midi_device_unref(node->midi_device);
        destroy(node, 1);
    }
}

void ResourcesTreeWidget::on_mouse_move(const MouseEvent *event) {
    if (event->action != MouseActionDown)
        return;

    List<Node *> stack;
    ok_or_panic(stack.append(root_node));

    while (stack.length() > 0) {
        Node *node = stack.pop();
        add_children_to_stack(stack, node);
        if (node->node_type == NodeTypeParent) {
            if (event->x >= node->icon_left && event->x < node->icon_left + icon_width + icon_spacing &&
                event->y >= node->top && event->y < node->bottom)
            {
                toggle_expansion(node);
                break;
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
        ok_or_panic(stack.append(child));
    }
}

void ResourcesTreeWidget::toggle_expansion(Node *node) {
    node->parent_data->expanded = !node->parent_data->expanded;
    node->icon_img = node->parent_data->expanded ? gui->img_minus : gui->img_plus;
}

bool ResourcesTreeWidget::should_draw_icon(Node *node) {
    if (!node->icon_img)
        return false;
    if (node->node_type == NodeTypeParent && node->parent_data->children.length() == 0)
        return false;
    return true;
}
