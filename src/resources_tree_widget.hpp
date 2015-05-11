#ifndef RESOURCES_TREE_WIDGET
#define RESOURCES_TREE_WIDGET

#include "widget.hpp"
#include "genesis.h"
#include "label.hpp"
#include "os.hpp"
#include "sunken_box.hpp"

class GuiWindow;
class Gui;
class SpritesheetImage;
class SettingsFile;
class ScrollBarWidget;

class ResourcesTreeWidget : public Widget {
public:
    ResourcesTreeWidget(GuiWindow *gui_window, SettingsFile *settings_file);
    ~ResourcesTreeWidget() override;

    void draw(const glm::mat4 &projection) override;

    void on_resize() override { update_model(); }
    void on_mouse_move(const MouseEvent *) override;
    void on_mouse_wheel(const MouseWheelEvent *event) override;
    bool on_key_event(const KeyEvent *) override;


    enum NodeType {
        NodeTypeParent,
        NodeTypePlaybackDevice,
        NodeTypeRecordingDevice,
        NodeTypeMidiDevice,
        NodeTypeSampleFile,
    };

    struct Node;
    struct NodeDisplay;
    struct ParentNode {
        List<Node *> children;
        bool expanded;
    };

    struct Node {
        Node *parent_node;
        NodeDisplay *display;
        String text;
        int indent_level;
        const SpritesheetImage *icon_img;
        NodeType node_type;
        // not adjusted for scroll position
        int top;
        int bottom;

        // these depend on node_type
        GenesisAudioDevice *audio_device;
        GenesisMidiDevice *midi_device;
        ParentNode *parent_data;
        OsDirEntry *dir_entry;
    };

    struct NodeDisplay {
        Node *node;
        Label *label;
        glm::mat4 label_model;
        glm::mat4 icon_model;
        glm::mat4 selected_model;
        // adjusted for scroll position
        int top;
        int bottom;
        int icon_left;
        int icon_top;
        int right;
    };

    GenesisContext *context;
    Gui *gui;
    glm::vec4 text_color;
    glm::vec4 selection_color;
    SunkenBox bg;
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
    SettingsFile *settings_file;
    List<NodeDisplay *> display_nodes;
    ScrollBarWidget *scroll_bar;
    Label *dummy_label; // so we know the height
    int display_node_count;
    glm::mat4 stencil_model;
    Node *selected_node;

    void update_model();

    Node *create_parent_node(Node *parent, const char *text);
    Node *create_playback_node();
    Node *create_record_node();
    Node *create_midi_node();
    Node *create_sample_file_node(Node *parent, OsDirEntry *entry);
    void destroy_node(Node *node);
    void pop_destroy_child(Node *node);
    void add_children_to_stack(List<Node *> &stack, Node *node);
    void toggle_expansion(Node *node);
    bool should_draw_icon(Node *node);
    void destroy_dir_cache();
    void delete_all_children(Node *node);
    void destroy_node_display(NodeDisplay *node_display);
    NodeDisplay * create_node_display(Node *node);
    void clear_display_nodes();
    void scan_dir_recursive(const ByteBuffer &dir, Node *parent_node);

    // must call update_model after calling these
    void scan_sample_dirs();
    void refresh_devices();

    void nav_sel_x(int dir);
    void nav_sel_y(int dir);
    Node * get_first_node();
    Node * get_last_node();
    void mouse_down_node(Node *node);
    void double_click_node(Node *node);
    bool is_node_expanded(Node *node);
};

#endif
