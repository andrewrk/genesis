#ifndef RESOURCES_TREE_WIDGET
#define RESOURCES_TREE_WIDGET

#include "widget.hpp"
#include "genesis.h"
#include "label.hpp"
#include "os.hpp"

class GuiWindow;
class Gui;
class SpritesheetImage;
class SettingsFile;

struct SampleDirCache {
    OsDirEntry *entry;
    List<SampleDirCache *> dirs;
    List<OsDirEntry *> files;
};

class ResourcesTreeWidget : public Widget {
public:
    ResourcesTreeWidget(GuiWindow *gui_window, SettingsFile *settings_file);
    ~ResourcesTreeWidget() override;

    void draw(const glm::mat4 &projection) override;

    void on_resize() override { update_model(); }
    void on_mouse_move(const MouseEvent *) override;


    enum NodeType {
        NodeTypeParent,
        NodeTypePlaybackDevice,
        NodeTypeRecordingDevice,
        NodeTypeMidiDevice,
        NodeTypeSampleDir,
        NodeTypeSampleFile,
    };

    struct Node;
    struct NodeDisplay;
    struct ParentNode {
        List<Node *> children;
        bool expanded;
    };

    struct Node {
        NodeDisplay *display;
        String text;
        const SpritesheetImage *icon_img;
        NodeType node_type;
        GenesisAudioDevice *audio_device;
        GenesisMidiDevice *midi_device;
        ParentNode *parent_data;
        Node *parent_node;
    };

    struct NodeDisplay {
        Node *node;
        Label *label;
        glm::mat4 label_model;
        glm::mat4 icon_model;
        int indent_level;
        int top;
        int bottom;
        int icon_left;
        int icon_top;
    };

    GenesisContext *context;
    Gui *gui;
    glm::vec4 bg_color;
    glm::vec4 text_color;
    glm::mat4 bg_model;
    Node *root_node;
    int padding_top;
    int padding_bottom;
    int padding_left;
    int padding_right;
    int icon_spacing;
    int icon_width;
    int icon_height;
    int item_padding_top;
    int item_padding_bottom;
    Node *playback_devices_root;
    Node *recording_devices_root;
    Node *midi_devices_root;
    Node *samples_root;
    List<Node *> update_model_stack;
    List<Node *> draw_stack;
    SettingsFile *settings_file;
    SampleDirCache *sample_dir_cache_root;
    List<NodeDisplay *> display_nodes;

    void update_model();

    Node *create_parent_node(Node *parent, const char *text);
    Node *create_playback_node();
    Node *create_record_node();
    Node *create_midi_node();
    void destroy_node(Node *node);
    void pop_destroy_child(Node *node);
    void add_children_to_stack(List<Node *> &stack, Node *node);
    void toggle_expansion(Node *node);
    bool should_draw_icon(Node *node);
    void scan_sample_dirs();
    void destroy_dir_cache();
};

#endif
