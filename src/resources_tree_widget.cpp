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
    padding_top(2),
    padding_bottom(0),
    padding_left(2),
    padding_right(0),
    icon_spacing(4),
    icon_width(8),
    icon_height(8),
    item_padding_top(4),
    item_padding_bottom(4)
{
    gui->attach_audio_device_callback(device_change_callback, this);
    gui->attach_midi_device_callback(device_change_callback, this);

    root_node = create_parent_node(nullptr, "");

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
    for (int i = root_node->parent_data->children.length() - 1; i >= 0; i -= 1) {
        Node *child = root_node->parent_data->children.at(i);
        ok_or_panic(draw_stack.append(child));
    }

    while (draw_stack.length() > 0) {
        Node *child = draw_stack.pop();
        glm::mat4 label_mvp = projection * child->label_model;
        child->label->draw(gui_window, label_mvp, text_color);
        if (child->icon_img) {
            glm::mat4 icon_mvp = projection * child->icon_model;
            gui->draw_image_color(gui_window, child->icon_img, icon_mvp, text_color);
        }
        switch (child->node_type) {
            case NodeTypeParent:
                if (child->parent_data->expanded) {
                    for (int i = child->parent_data->children.length() - 1; i >= 0; i -= 1) {
                        Node *next_child = child->parent_data->children.at(i);
                        ok_or_panic(draw_stack.append(next_child));
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
    for (int i = root_node->parent_data->children.length() - 1; i >= 0; i -= 1) {
        Node *child = root_node->parent_data->children.at(i);
        ok_or_panic(update_model_stack.append(child));
        child->indent_level = 0;
    }

    int next_top = padding_top;

    while (update_model_stack.length() > 0) {
        Node *child = update_model_stack.pop();
        switch (child->node_type) {
            case NodeTypeParent:
                {
                    child->label->update();
                    child->top = next_top;
                    next_top += item_padding_top + child->label->height() + item_padding_bottom;
                    child->bottom = next_top;

                    int label_left = left + padding_left + (icon_width + icon_spacing) * (child->indent_level + 1);
                    int label_top = top + child->top + item_padding_top;
                    child->label_model = glm::translate(
                                    glm::mat4(1.0f),
                                    glm::vec3(label_left, label_top, 0.0f));
                    if (child->icon_img) {
                        int icon_left = left + padding_left + (icon_width + icon_spacing) * child->indent_level;
                        int icon_top = top + child->top + (child->bottom - child->top) / 2 - icon_height / 2;
                        float icon_scale_width = icon_width / (float)child->icon_img->width;
                        float icon_scale_height = icon_height / (float)child->icon_img->height;
                        child->icon_model = glm::scale(
                                        glm::translate(
                                            glm::mat4(1.0f),
                                            glm::vec3(icon_left, icon_top, 0.0f)),
                                        glm::vec3(icon_scale_width, icon_scale_height, 1.0f));
                    }
                    if (child->parent_data->expanded) {
                        for (int i = child->parent_data->children.length() - 1; i >= 0; i -= 1) {
                            Node *next_child = child->parent_data->children.at(i);
                            next_child->indent_level = child->indent_level + 1;
                            ok_or_panic(update_model_stack.append(next_child));
                        }
                    }
                    break;
                }
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
