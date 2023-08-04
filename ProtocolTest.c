/** 
 * @file ProtocolTest.c
 * @date 2023/7/17~2023/8/11
 * @author Minho Shin (smh9800@g.skku.edu)
 * @version 0.0.0.1
 * @brief Fastech 프로그램의 Protocol Test 구현을 위한 프로그램
 * @details C언어와 GTK3(라즈비안(데비안11) 호환을 위해서), GLADE(UI XML->.glade파일) 사용
 * Ethernet 부분(Ezi Servo Plus-E 모델용)만 구현, 
 * 라이브러리로 분리할 만한 기본 함수, GUI프로그램 구현 함수가 섞인 상태
 * @warning 동작 시 예외처리가 제대로 안되어있으니 정확한 절차로만 작동시킬것
 */

#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include "ReturnCodes_Define.h"


/************************************************************************************************************************************
 ********************************나중에 라이브러리로 뺄 FASTECH 라이브러리와 같은 기능의 함수*************************************************
 ************************************************************************************************************************************/
 
typedef uint8_t BYTE;
typedef uint32_t DWORD;
typedef char* LPSTR;

#define BUFFER_SIZE 258
#define DATA_SIZE 253
#define PORT_UDP 3001 //UDP GUI
#define PORT_TCP 2001 //TCP GUI
#define TIMEOUT_SECONDS 2

int client_socket;
struct sockaddr_in server_addr;

static BYTE header, sync_no, frame_type;
static BYTE data[DATA_SIZE];
static BYTE buffer[BUFFER_SIZE];

char *protocol;
bool show = TRUE;

bool FAS_Connect(BYTE sb1, BYTE sb2, BYTE sb3, BYTE sb4, int iBdID);
bool FAS_ConnectTCP(BYTE sb1, BYTE sb2, BYTE sb3, BYTE sb4, int iBdID);

void FAS_Close(int iBdID);

int FAS_ServoEnable(int iBdID, bool bOnOff);
int FAS_MoveOriginSingleAxis(int iBdID);
int FAS_MoveStop(int iBdID);
int FAS_MoveVelocity(int iBdID, DWORD IVelocity, int iVelDir);
int FAS_GetboardInfo(int iBdID, BYTE pType, LPSTR LpBuff, int nBuffSize);
int FAS_GetMotorInfo(int iBdID, BYTE pType, LPSTR LpBuff, int nBuffSize);
int FAS_GetEncoder(int iBdID, BYTE pType, LPSTR LpBuff, int nBuffSize);
int FAS_GetFirmwareInfo(int iBdID, BYTE pType, LPSTR LpBuff, int nBuffSize);
int FAS_GetSlaveInfoEx(int iBdID, BYTE pType, LPSTR LpBuff, int nBuffSize);
int FAS_SaveAllParameters(int iBdID);
int FAS_ServoAlarmReset(int iBdID);
int FAS_EmergencyStop(int iBdID);
int FAS_GetAlarmType(int iBdID);

/************************************************************************************************************************************
 ***************************GUI 프로그램의 버튼 등 구성요소들에서 사용하는 callback등 여러 함수***********************************************
 ************************************************************************************************************************************/
static void on_button_connect_clicked(GtkButton *button, gpointer user_data);
static void on_button_send_clicked(GtkButton *button, gpointer user_data);

static void on_button_record1_clicked(GtkButton *button, gpointer user_data);
static void on_button_record2_clicked(GtkButton *button, gpointer user_data);
static void on_button_record3_clicked(GtkButton *button, gpointer user_data);
static void on_button_record4_clicked(GtkButton *button, gpointer user_data);

static void on_button_transfer1_clicked(GtkButton *button, gpointer user_data);
static void on_button_transfer2_clicked(GtkButton *button, gpointer user_data);
static void on_button_transfer3_clicked(GtkButton *button, gpointer user_data);
static void on_button_transfer4_clicked(GtkButton *button, gpointer user_data);

static void on_combo_protocol_changed(GtkComboBoxText *combo_text, gpointer user_data);
static void on_combo_command_changed(GtkComboBox *combo_id, gpointer user_data);
static void on_combo_data1_changed(GtkComboBox *combo_id, gpointer user_data);
static void on_combo_direction_changed(GtkComboBox *combo_id, gpointer user_data);

static void on_check_autosync_toggled(GtkToggleButton *togglebutton, gpointer user_data);
static void on_check_fastech_toggled(GtkToggleButton *togglebutton, gpointer user_data);
static void on_check_showsend_toggled(GtkToggleButton *togglebutton, gpointer user_data);

gboolean on_text_frame_key_release_event(GtkWidget *widget, GdkEventKey *event, gpointer user_data);
/************************************************************************************************************************************
 ******************************************************* 편의상 만든 함수 **************************************************************
 ************************************************************************************************************************************/
 
GtkWidget *text_sendbuffer;
GtkWidget *text_monitor1;
GtkWidget *text_monitor2;
GtkWidget *text_autosync;
GtkWidget *text_frame;
GtkWidget *record_command1;
GtkWidget *record_command2;
GtkWidget *record_command3;
GtkWidget *record_command4;

GtkTextBuffer *sendbuffer_buffer;
GtkTextBuffer *monitor1_buffer;
GtkTextBuffer *monitor2_buffer;
GtkTextBuffer *autosync_buffer;
GtkTextBuffer *frame_buffer;
GtkTextBuffer *record1_buffer;
GtkTextBuffer *record2_buffer;
GtkTextBuffer *record3_buffer;
GtkTextBuffer *record4_buffer;

GtkLabel *label_status;
GtkLabel *label_time; 
 
void syno_no_update();
char* get_time();
void handle_alarm(int signum);
void send_packet(BYTE *byte_array);
void send_packetTCP(BYTE *byte_array);
void library_interface();
char *command_interface();
char *FMM_interface(FMM_ERROR error);
void print_buffer(uint8_t *array, size_t size);
char* array_to_string(const unsigned char *array, int size);


 /**@brief Main 함수*/
int main(int argc, char *argv[]) {
    GtkBuilder *builder;
    GtkWidget *window;
    GObject *button;
    GtkComboBoxText *combo_text;
    GtkComboBox* combo_id;
    GObject *checkbox;
    GError *error = NULL;
    
    srand(time(NULL));
    signal(SIGALRM, handle_alarm);

    header = 0xAA;
    sync_no = (BYTE)(rand() % 256);
    
    // GTK 초기화
    gtk_init(&argc, &argv);

    // GtkBuilder 생성
    builder = gtk_builder_new();

    // XML file에서 UI불러오기
    if (!gtk_builder_add_from_file(builder, "ProtocolTest.glade", &error)) {
        g_printerr("Error loading UI file: %s\n", error->message);
        g_clear_error(&error);
        return 1;
    }

    // Window 생성
    window = GTK_WIDGET(gtk_builder_get_object(builder, "window"));
    gtk_widget_show_all(window);
    
    GtkStack *stk2 = GTK_STACK(gtk_builder_get_object(builder, "stk2"));
    
    text_monitor1 = GTK_WIDGET(gtk_builder_get_object(builder, "text_monitor1"));
    monitor1_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_monitor1));
    text_monitor2 = GTK_WIDGET(gtk_builder_get_object(builder, "text_monitor2"));
    monitor2_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_monitor2));
    text_sendbuffer = GTK_WIDGET(gtk_builder_get_object(builder, "text_sendbuffer"));
    sendbuffer_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_sendbuffer));
    text_autosync = GTK_WIDGET(gtk_builder_get_object(builder, "text_autosync"));
    autosync_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_autosync));
    
    record_command1 = GTK_WIDGET(gtk_builder_get_object(builder, "record_command1"));
    record1_buffer= gtk_text_view_get_buffer(GTK_TEXT_VIEW(record_command1));
    record_command2 = GTK_WIDGET(gtk_builder_get_object(builder, "record_command2"));
    record2_buffer= gtk_text_view_get_buffer(GTK_TEXT_VIEW(record_command2));
    record_command3 = GTK_WIDGET(gtk_builder_get_object(builder, "record_command3"));
    record3_buffer= gtk_text_view_get_buffer(GTK_TEXT_VIEW(record_command3));
    record_command4 = GTK_WIDGET(gtk_builder_get_object(builder, "record_command4"));
    record4_buffer= gtk_text_view_get_buffer(GTK_TEXT_VIEW(record_command4));
    
    text_frame = GTK_WIDGET(gtk_builder_get_object(builder, "text_frame"));
    frame_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_frame));
    g_signal_connect(text_frame, "key-release-event", G_CALLBACK(on_text_frame_key_release_event), NULL);
    
    label_status = GTK_LABEL(gtk_builder_get_object(builder, "label_status"));
    label_time = GTK_LABEL(gtk_builder_get_object(builder, "label_time"));
    
    // callback 함수 연결, user_data를 빌더로 사용함
    button = gtk_builder_get_object(builder, "button_connect");
    g_signal_connect(button, "clicked", G_CALLBACK(on_button_connect_clicked), builder);
    button = gtk_builder_get_object(builder, "button_send");
    g_signal_connect(button, "clicked", G_CALLBACK(on_button_send_clicked), builder);
    
    button = gtk_builder_get_object(builder, "button_record1");
    g_signal_connect(button, "clicked", G_CALLBACK(on_button_record1_clicked), builder);
    button = gtk_builder_get_object(builder, "button_record2");
    g_signal_connect(button, "clicked", G_CALLBACK(on_button_record2_clicked), builder);
    button = gtk_builder_get_object(builder, "button_record3");
    g_signal_connect(button, "clicked", G_CALLBACK(on_button_record3_clicked), builder);
    button = gtk_builder_get_object(builder, "button_record4");
    g_signal_connect(button, "clicked", G_CALLBACK(on_button_record4_clicked), builder);
    
    button = gtk_builder_get_object(builder, "button_transfer1");
    g_signal_connect(button, "clicked", G_CALLBACK(on_button_transfer1_clicked), NULL);
    button = gtk_builder_get_object(builder, "button_transfer2");
    g_signal_connect(button, "clicked", G_CALLBACK(on_button_transfer2_clicked), NULL);
    button = gtk_builder_get_object(builder, "button_transfer3");
    g_signal_connect(button, "clicked", G_CALLBACK(on_button_transfer3_clicked), NULL);
    button = gtk_builder_get_object(builder, "button_transfer4");
    g_signal_connect(button, "clicked", G_CALLBACK(on_button_transfer4_clicked), NULL);
    
    combo_text = GTK_COMBO_BOX_TEXT(gtk_builder_get_object(builder, "combo_protocol"));
    g_signal_connect(combo_text, "changed", G_CALLBACK(on_combo_protocol_changed), NULL);
    
    combo_id = GTK_COMBO_BOX(gtk_builder_get_object(builder, "combo_command"));
    g_signal_connect(combo_id, "changed", G_CALLBACK(on_combo_command_changed), stk2);
    combo_id = GTK_COMBO_BOX(gtk_builder_get_object(builder, "combo_data1"));
    g_signal_connect(combo_id, "changed", G_CALLBACK(on_combo_data1_changed), NULL);
    combo_id = GTK_COMBO_BOX(gtk_builder_get_object(builder, "combo_direction"));
    g_signal_connect(combo_id, "changed", G_CALLBACK(on_combo_direction_changed), builder);


    checkbox = gtk_builder_get_object(builder, "check_autosync");
    g_signal_connect(checkbox, "toggled", G_CALLBACK(on_check_autosync_toggled), NULL);
    checkbox = gtk_builder_get_object(builder, "check_fastech");
    g_signal_connect(checkbox, "toggled", G_CALLBACK(on_check_fastech_toggled), NULL);
    checkbox = gtk_builder_get_object(builder, "check_showsend");
    g_signal_connect(checkbox, "toggled", G_CALLBACK(on_check_showsend_toggled), NULL);
    
    char sync_str[4];
    sprintf(sync_str, "%u", sync_no);
    
    gtk_text_buffer_set_text(autosync_buffer, sync_str, -1);
    
    GObject *button_send = gtk_builder_get_object(builder, "button_send");
    gtk_widget_set_sensitive(GTK_WIDGET(button_send), FALSE);

    // 메인 루프 실행
    // Start the GTK main loop
    gtk_main();

    return 0;
}

/************************************************************************************************************************************
 ********************************GUI 프로그램의 버튼 등 구성요소들에서 사용하는 callback함수*************************************************
 ************************************************************************************************************************************/
 
 /**@brief Connect버튼의 callback*/
static void on_button_connect_clicked(GtkButton *button, gpointer user_data) {
    BYTE sb1, sb2, sb3, sb4;
    
    // Get the GtkBuilder object passed as user data
    GtkBuilder *builder = GTK_BUILDER(user_data);

    // Get the label of the button
    const char *label_text = gtk_button_get_label(button);
    GObject *button_send = gtk_builder_get_object(builder, "button_send");

    // Get the entry widget by its ID
    GtkEntry *entry_ip = GTK_ENTRY(gtk_builder_get_object(builder, "entry_ip"));

    // Get the entered text from the entry
    const char *ip_text = gtk_entry_get_text(entry_ip);

    // Check if the IP is valid (For a simple example, let's assume it's valid if it's not empty)
    if (g_strcmp0(ip_text, "") != 0) {
        g_print("IP: %s\n", ip_text);

        // Parse and store IP address in BYTE format
        char *ip_copy = g_strdup(ip_text); // 문자열 복사
        char *token, *saveptr;

        for (int i = 0; i < 4; i++) {
            token = strtok_r(i == 0 ? ip_copy : NULL, ".", &saveptr);
            if (token == NULL) {
                g_print("Invalid IP format: %s\n", ip_text);
                g_free(ip_copy);
                return;
            }
            BYTE byte_val = atoi(token);
            switch (i) {
                case 0:
                    sb1 = byte_val;
                    break;
                case 1:
                    sb2 = byte_val;
                    break;
                case 2:
                    sb3 = byte_val;
                    break;
                case 3:
                    sb4 = byte_val;
                    break;
            }
        }
        g_print("Parsed IP: %d.%d.%d.%d\n", sb1, sb2, sb3, sb4);
        g_free(ip_copy);
    }
    else {
            g_print("Please enter a valid IP.\n");
    }
        
    // Check the current label and update it accordingly
    if (strcmp(label_text, "Connect") == 0)
    {
        g_print("Selected Protocol: %s\n", protocol);
        
        if(strcmp(protocol, "TCP") == 0){
            if(FAS_ConnectTCP(sb1, sb2, sb3, sb4, 0)){
                gtk_button_set_label(button, "Disconn");
                gtk_widget_set_sensitive(GTK_WIDGET(button_send), TRUE);
            }
        }
        else if(strcmp(protocol, "UDP") == 0){
            if(FAS_Connect(sb1, sb2, sb3, sb4, 0)){
                gtk_button_set_label(button, "Disconn");
                gtk_widget_set_sensitive(GTK_WIDGET(button_send), TRUE);
            }
        }
        else if(protocol != NULL){
            g_print("Select Protocol\n");
        }
    }
    else if (strcmp(label_text, "Disconn") == 0)
    {
        FAS_Close(0);
        gtk_button_set_label(button, "Connect");
        gtk_widget_set_sensitive(GTK_WIDGET(button_send), FALSE);
    }
}

 /**@brief Send버튼의 callback*/
static void on_button_send_clicked(GtkButton *button, gpointer user_data){
    
    if(strcmp(protocol, "TCP") == 0){
        send_packetTCP(buffer);
    }
    else if(strcmp(protocol, "UDP") == 0){
        send_packet(buffer);
    }
    else if(protocol != NULL){
        g_print("Select Protocol\n");
    }

}

 /**@brief TCP/UDP 프로토콜 선택 콤보박스의 callback*/
static void on_combo_protocol_changed(GtkComboBoxText *combo_text, gpointer user_data) {
    protocol = gtk_combo_box_text_get_active_text(combo_text);
    if (protocol != NULL) {
        g_print("Selected Protocol: %s\n", protocol);
    }
}

 /**@brief 명령어 콤보박스 combo_command의 callback*/
static void on_combo_command_changed(GtkComboBox *combo_id, gpointer user_data) {
    const gchar *selected_id = gtk_combo_box_get_active_id(combo_id);
    
    gtk_label_set_text(label_status, "Ready");
    if (selected_id != NULL) {
        GtkStack *stk2 = GTK_STACK(user_data);

        // 선택한 옵션에 따라 보여지는 페이지를 변경
        if (g_strcmp0(selected_id, "0x2A") == 0) {
            gtk_stack_set_visible_child_name(stk2, "page1");
        } 
        else if (g_strcmp0(selected_id, "0x37") == 0) {
            gtk_stack_set_visible_child_name(stk2, "page2");
        }
        else{
            gtk_stack_set_visible_child_name(stk2, "page0");
        }
    }
    
    if (selected_id  != NULL) {
        g_print("Selected Command: %s\n", selected_id);
        char* endptr;
        unsigned long int value = strtoul(selected_id, &endptr, 16);
        if (*endptr == '\0' && value <= UINT8_MAX) {
            frame_type = (uint8_t)value;
        } else {
            g_print("Invalid input: %s\n", selected_id);
        }
    } else {
        g_print("No item selected.\n");
    }
    g_print("Converted Frame: %X \n", frame_type);
    library_interface();
}

 /**@brief 명령어 콤보박스 combo_data1의 callback*/
static void on_combo_data1_changed(GtkComboBox *combo_id, gpointer user_data) {
    const gchar *selected_id = gtk_combo_box_get_active_id(combo_id);
    memset(&data, 0, sizeof(data));
    if (selected_id != NULL) {
        g_print("Selected Data: %s\n", selected_id);
        char* endptr;
        unsigned long int value = strtoul(selected_id, &endptr, 16);
        if (*endptr == '\0' && value <= UINT8_MAX) {
            data[0] = (uint8_t)value;
        } else {
            g_print("Invalid input: %s\n", selected_id);
        }
    } else {
        g_print("No item selected.\n");
    }
    g_print("Converted Data: %X \n", data[0]);
    library_interface();
}

 /**@brief AutoSync 체크박스의 callback*/
static void on_check_autosync_toggled(GtkToggleButton *togglebutton, gpointer user_data) {
    gboolean is_checked = gtk_toggle_button_get_active(togglebutton);
    if (is_checked) {
        sync_no = (BYTE)(rand() % 256);
        g_print("Auto Sync Enabled Sync No: %X \n",sync_no);
    } else {
        sync_no = 0x00;
        g_print("Auto Sync Disabled Sync No: %X \n",sync_no);
    }
}

 /**@brief 보낸 패킷 표시 체크박스의 callback*/
static void on_check_showsend_toggled(GtkToggleButton *togglebutton, gpointer user_data) {
    gboolean is_checked = gtk_toggle_button_get_active(togglebutton);
    if (is_checked) {
        show = TRUE;
    } else {
        show = FALSE;
    }
}

 /**@brief FASTECH 프로토콜 체크박스의 callback*/
static void on_check_fastech_toggled(GtkToggleButton *togglebutton, gpointer user_data) {
    gboolean is_checked = gtk_toggle_button_get_active(togglebutton);
    if (is_checked) {
        header = 0xAA;
        g_print("FASTECH Protocol header: %X \n",header);
    } else {
        header = 0x00;
        g_print("USER Protocol header: %X \n",header);
    }
}

 /**@brief MoveVelocity에서 방향 선택 콤보박스의 callback*/
static void on_combo_direction_changed(GtkComboBox *combo_id, gpointer user_data) {
    memset(&data, 0, sizeof(data));
    // Get the GtkBuilder object passed as user data
    GtkBuilder *builder = GTK_BUILDER(user_data);

    // Get the entry widget by its ID
    GtkEntry *entry_speed = GTK_ENTRY(gtk_builder_get_object(builder, "entry_speed"));
    
    const gchar *selected_id = gtk_combo_box_get_active_id(combo_id);
    const gchar *text = gtk_entry_get_text(GTK_ENTRY(entry_speed));
    
    int value = atoi(text);
    
    for (int i = 0; i < 4; i++) {
        data[i] = (value >> (8 * i)) & 0xFF;
    }

    if (selected_id != NULL) {
        g_print("Selected Data: %s\n", selected_id);
        char* endptr;
        unsigned long int value = strtoul(selected_id, &endptr, 16);
        if (*endptr == '\0' && value <= UINT8_MAX) {
            data[4] = (uint8_t)value;
        } else {
            g_print("Invalid input: %s\n", selected_id);
        }
    } else {
        g_print("No item selected.\n");
    }
    print_buffer(data, 5);
    library_interface();
}

 /**@brief Frame칸에 명령어를 입력할 때 생기는 callback*/
gboolean on_text_frame_key_release_event(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
    GtkTextBuffer *frame_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(widget));

    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(frame_buffer, &start, &end);
    char *input = gtk_text_buffer_get_text(frame_buffer, &start, &end, FALSE);

    g_print("Current Text: %s\n", input);

    int inputLength = strlen(input);
    int outputSize = 0;
    uint8_t *output = NULL;

    // "[2A 01 12]"처럼 HEX 형식인지 확인
    if (inputLength >= 3 && input[0] == '[' && input[inputLength - 1] == ']') {
        // "2A 01 12" 부분 추출
        char hexPart[100];
        strncpy(hexPart, input + 1, inputLength - 2);
        hexPart[inputLength - 2] = '\0';

        // 16진수 문자열을 숫자로 변환하여 동적 메모리 할당 후 저장
        char *token;
        int index = 0;
        token = strtok(hexPart, " ");
        while (token != NULL) {
            output = (uint8_t *)realloc(output, (index + 1) * sizeof(uint8_t));
            output[index++] = (uint8_t)strtol(token, NULL, 16);
            token = strtok(NULL, " ");
        }

        outputSize = index;
    }
    // "112 123 123" 처럼 10진수 형식인 경우
    else if (input[0] != '[') {
        // 10진수 문자열을 숫자로 변환하여 동적 메모리 할당 후 저장
        char *token;
        int index = 0;
        token = strtok(input, " ");
        while (token != NULL) {
            output = (uint8_t *)realloc(output, (index + 1) * sizeof(uint8_t));
            output[index++] = (uint8_t)atoi(token);
            token = strtok(NULL, " ");
        }
        outputSize = index;
    }
    else{
        g_free(output);
    }

    printf("Parsed Hex: ");
    for (int i = 0; i < outputSize; i++) {
        printf("%02X ", output[i]);
    }
    printf("\n");
    
    g_free(input);

    if (output != NULL) {
        buffer[0] = header;
        buffer[1] = outputSize + 2;
        buffer[2] = sync_no;
        memcpy(&buffer[4], output, outputSize); // outputSize만큼 복사
        free(output);

        char *text = array_to_string(buffer, buffer[1] + 2);
        gtk_text_buffer_set_text(sendbuffer_buffer, text, -1);
        g_free(text);
    } else {
        gtk_text_buffer_set_text(sendbuffer_buffer, "", -1);
        memset(buffer, 0, sizeof(buffer));
    }

    return FALSE;
}

static void on_button_record1_clicked(GtkButton *button, gpointer user_data) {
    GtkTextIter start, end;
    gtk_text_buffer_get_start_iter(sendbuffer_buffer, &start);
    gtk_text_buffer_get_end_iter(sendbuffer_buffer, &end);
    gchar *text = gtk_text_buffer_get_text(sendbuffer_buffer, &start, &end, FALSE);
    
    gtk_text_buffer_set_text(record1_buffer, text, -1);
}

static void on_button_record2_clicked(GtkButton *button, gpointer user_data) {
    GtkTextIter start, end;
    gtk_text_buffer_get_start_iter(sendbuffer_buffer, &start);
    gtk_text_buffer_get_end_iter(sendbuffer_buffer, &end);
    gchar *text = gtk_text_buffer_get_text(sendbuffer_buffer, &start, &end, FALSE);
    
    gtk_text_buffer_set_text(record2_buffer, text, -1);
}

static void on_button_record3_clicked(GtkButton *button, gpointer user_data) {
    GtkTextIter start, end;
    gtk_text_buffer_get_start_iter(sendbuffer_buffer, &start);
    gtk_text_buffer_get_end_iter(sendbuffer_buffer, &end);
    gchar *text = gtk_text_buffer_get_text(sendbuffer_buffer, &start, &end, FALSE);
    
    gtk_text_buffer_set_text(record3_buffer, text, -1);
}

static void on_button_record4_clicked(GtkButton *button, gpointer user_data) {
    GtkTextIter start, end;
    gtk_text_buffer_get_start_iter(sendbuffer_buffer, &start);
    gtk_text_buffer_get_end_iter(sendbuffer_buffer, &end);
    gchar *text = gtk_text_buffer_get_text(sendbuffer_buffer, &start, &end, FALSE);
    
    gtk_text_buffer_set_text(record4_buffer, text, -1);
}


// 전송 버튼을 누를 때 호출되는 콜백 함수
static void on_button_transfer1_clicked(GtkButton *button, gpointer user_data) {
    gtk_label_set_text(label_status, "Sending");
    
    GtkTextIter start, end;
    gtk_text_buffer_get_start_iter(record1_buffer, &start);
    gtk_text_buffer_get_end_iter(record1_buffer, &end);
    gchar *text = gtk_text_buffer_get_text(record1_buffer, &start, &end, FALSE);
    
    gtk_text_buffer_set_text(sendbuffer_buffer, text, -1);
    // 텍스트를 띄어쓰기를 기준으로 16진수로 변환하여 uint8_t 배열에 저장합니다.
    BYTE byte_array[258];
    int byte_count = 0;
    gchar **tokens = g_strsplit(text, " ", -1); // 띄어쓰기로 문자열 분할
    for (int i = 0; tokens[i] != NULL; i++) {
        byte_array[byte_count] = (BYTE)g_ascii_strtoull(tokens[i], NULL, 16);
        byte_count++;
    }
    g_strfreev(tokens);

    // 변환된 16진수 값을 10진수로 출력해봅니다.
    for (int i = 0; i < byte_count; i++) {
        printf("%02X ", byte_array[i]);
    }
    printf("\n");
    
    if(strcmp(protocol, "TCP") == 0){
        send_packetTCP(byte_array);
    }
    else if(strcmp(protocol, "UDP") == 0){
        send_packet(byte_array);
    }
    else if(protocol != NULL){
        g_print("Select Protocol\n");
    }
}

// 전송 버튼을 누를 때 호출되는 콜백 함수
static void on_button_transfer2_clicked(GtkButton *button, gpointer user_data) {
    gtk_label_set_text(label_status, "Sending");
    
    GtkTextIter start, end;
    gtk_text_buffer_get_start_iter(record2_buffer, &start);
    gtk_text_buffer_get_end_iter(record2_buffer, &end);
    gchar *text = gtk_text_buffer_get_text(record2_buffer, &start, &end, FALSE);
    
    gtk_text_buffer_set_text(sendbuffer_buffer, text, -1);
    // 텍스트를 띄어쓰기를 기준으로 16진수로 변환하여 uint8_t 배열에 저장합니다.
    BYTE byte_array[258];
    int byte_count = 0;
    gchar **tokens = g_strsplit(text, " ", -1); // 띄어쓰기로 문자열 분할
    for (int i = 0; tokens[i] != NULL; i++) {
        byte_array[byte_count] = (BYTE)g_ascii_strtoull(tokens[i], NULL, 16);
        byte_count++;
    }
    g_strfreev(tokens);

    // 변환된 16진수 값을 10진수로 출력해봅니다.
    for (int i = 0; i < byte_count; i++) {
        printf("%02X ", byte_array[i]);
    }
    printf("\n");
    
    if(strcmp(protocol, "TCP") == 0){
        send_packetTCP(byte_array);
    }
    else if(strcmp(protocol, "UDP") == 0){
        send_packet(byte_array);
    }
    else if(protocol != NULL){
        g_print("Select Protocol\n");
    }
}

// 전송 버튼을 누를 때 호출되는 콜백 함수
static void on_button_transfer3_clicked(GtkButton *button, gpointer user_data) {
    gtk_label_set_text(label_status, "Sending");
    
    GtkTextIter start, end;
    gtk_text_buffer_get_start_iter(record3_buffer, &start);
    gtk_text_buffer_get_end_iter(record3_buffer, &end);
    gchar *text = gtk_text_buffer_get_text(record3_buffer, &start, &end, FALSE);
    
    gtk_text_buffer_set_text(sendbuffer_buffer, text, -1);
    // 텍스트를 띄어쓰기를 기준으로 16진수로 변환하여 uint8_t 배열에 저장합니다.
    BYTE byte_array[258];
    int byte_count = 0;
    gchar **tokens = g_strsplit(text, " ", -1); // 띄어쓰기로 문자열 분할
    for (int i = 0; tokens[i] != NULL; i++) {
        byte_array[byte_count] = (BYTE)g_ascii_strtoull(tokens[i], NULL, 16);
        byte_count++;
    }
    g_strfreev(tokens);

    // 변환된 16진수 값을 10진수로 출력해봅니다.
    for (int i = 0; i < byte_count; i++) {
        printf("%02X ", byte_array[i]);
    }
    printf("\n");
    
    if(strcmp(protocol, "TCP") == 0){
        send_packetTCP(byte_array);
    }
    else if(strcmp(protocol, "UDP") == 0){
        send_packet(byte_array);
    }
    else if(protocol != NULL){
        g_print("Select Protocol\n");
    }
}

// 전송 버튼을 누를 때 호출되는 콜백 함수
static void on_button_transfer4_clicked(GtkButton *button, gpointer user_data) {
    gtk_label_set_text(label_status, "Sending");
    
    GtkTextIter start, end;
    gtk_text_buffer_get_start_iter(record4_buffer, &start);
    gtk_text_buffer_get_end_iter(record4_buffer, &end);
    gchar *text = gtk_text_buffer_get_text(record4_buffer, &start, &end, FALSE);
    
    gtk_text_buffer_set_text(sendbuffer_buffer, text, -1);
    // 텍스트를 띄어쓰기를 기준으로 16진수로 변환하여 uint8_t 배열에 저장합니다.
    BYTE byte_array[258];
    int byte_count = 0;
    gchar **tokens = g_strsplit(text, " ", -1); // 띄어쓰기로 문자열 분할
    for (int i = 0; tokens[i] != NULL; i++) {
        byte_array[byte_count] = (BYTE)g_ascii_strtoull(tokens[i], NULL, 16);
        byte_count++;
    }
    g_strfreev(tokens);

    // 변환된 16진수 값을 10진수로 출력해봅니다.
    for (int i = 0; i < byte_count; i++) {
        printf("%02X ", byte_array[i]);
    }
    printf("\n");
    
    if(strcmp(protocol, "TCP") == 0){
        send_packetTCP(byte_array);
    }
    else if(strcmp(protocol, "UDP") == 0){
        send_packet(byte_array);
    }
    else if(protocol != NULL){
        g_print("Select Protocol\n");
    }
}
/************************************************************************************************************************************
 ********************************나중에 라이브러리로 뺄 FASTECH 라이브러리와 같은 기능의 함수*************************************************
 ************************************************************************************************************************************/
 
 /**@brief UDP 연결 시 사용
  * @param BYTE sb1,sb2,sb3,sb4 IPv4주소 입력 시 각 자리
  * @param int iBdID 드라이브 ID
  * @return boolean 성공시 TRUE 실패시 FALSE*/
bool FAS_Connect(BYTE sb1, BYTE sb2, BYTE sb3, BYTE sb4, int iBdID){
    char SERVER_IP[16]; //최대 길이 가정 "xxx.xxx.xxx.xxx\0" 
    snprintf(SERVER_IP, sizeof(SERVER_IP), "%u.%u.%u.%u", sb1, sb2, sb3, sb4);
    
    // Create socket
    if ((client_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed\n");
        close(client_socket);
        return FALSE;
    }
    else{
        g_print("Socket created\n");
    }

    memset(&server_addr, 0, sizeof(server_addr));
    
    // Configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT_UDP);
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported\n");
        close(client_socket);
        return FALSE;
    }
    else{
        g_print("Valid address\n");
    }
    return TRUE;
}

 /**@brief TCP 연결 시 사용
  * @param BYTE sb1,sb2,sb3,sb4 IPv4주소 입력 시 각 자리
  * @param int iBdID 드라이브 ID
  * @return boolean 성공시 TRUE 실패시 FALSE*/
bool FAS_ConnectTCP(BYTE sb1, BYTE sb2, BYTE sb3, BYTE sb4, int iBdID){
    
     char SERVER_IP[16]; // 최대 길이 가정 "xxx.xxx.xxx.xxx\0"
    snprintf(SERVER_IP, sizeof(SERVER_IP), "%u.%u.%u.%u", sb1, sb2, sb3, sb4);

    // Create socket
    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket creation failed");
        close(client_socket);
        return false;
    } else {
        g_print("Socket created\n");
    }

    // Prepare the sockaddr_in structure
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    server_addr.sin_port = htons(PORT_TCP);

    // Set the timer for TIMEOUT_SECONDS seconds
    alarm(TIMEOUT_SECONDS);

    // Connect to the server (blocking call, but limited by the timer)
    int result = connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr));

    // Cancel the timer
    alarm(0);

    if (result == -1) {
        perror("Connection failed");
        close(client_socket);
        return false;
    } else {
        g_print("Connection Success\n");
    }

    return true;
}

 /**@brief 연결 해제 시 사용
  * @param int iBdID 드라이브 ID */
void FAS_Close(int iBdID){
    close(client_socket);
}

 /**@brief 해당보드의 정보
  * @param int iBdID 드라이브 ID
  * @param BYTE pType 모터의 Type
  * @param LPSTR LpBuff Motor정보를 받을 문자열
  * @param int nBuffSize 버퍼의 사이즈 */
int FAS_GetboardInfo(int iBdID, BYTE pType, LPSTR LpBuff, int nBuffSize){
    buffer[0] = header; buffer[1] = 0x03; buffer[2] = sync_no; buffer[3] = 0x00; buffer[4] = frame_type;
    return 0;
}

 /**@brief 해당모터의 정보
  * @param int iBdID 드라이브 ID
  * @param BYTE pType 모터의 Type
  * @param LPSTR LpBuff Motor정보를 받을 문자열
  * @param int nBuffSize 버퍼의 사이즈 */
int FAS_GetMotorInfo(int iBdID, BYTE pType, LPSTR LpBuff, int nBuffSize){
    buffer[0] = header; buffer[1] = 0x03; buffer[2] = sync_no; buffer[3] = 0x00; buffer[4] = frame_type;
    return 0;
}

 /**@brief 해당엔코더의 정보
  * @param int iBdID 드라이브 ID
  * @param BYTE pType 모터의 Type
  * @param LPSTR LpBuff Motor정보를 받을 문자열
  * @param int nBuffSize 버퍼의 사이즈 */
int FAS_GetEncoder(int iBdID, BYTE pType, LPSTR LpBuff, int nBuffSize){
    buffer[0] = header; buffer[1] = 0x03; buffer[2] = sync_no; buffer[3] = 0x00; buffer[4] = frame_type;
    return 0;
}

 /**@brief 펌웨어의 정보
  * @param int iBdID 드라이브 ID
  * @param BYTE pType 모터의 Type
  * @param LPSTR LpBuff Motor정보를 받을 문자열
  * @param int nBuffSize 버퍼의 사이즈 */
int FAS_GetFirmwareInfo(int iBdID, BYTE pType, LPSTR LpBuff, int nBuffSize){
    buffer[0] = header; buffer[1] = 0x03; buffer[2] = sync_no; buffer[3] = 0x00; buffer[4] = frame_type;
    return 0;
}

 /**@brief 해당보드의 정보
  * @param int iBdID 드라이브 ID
  * @param BYTE pType 모터의 Type
  * @param LPSTR LpBuff Motor정보를 받을 문자열
  * @param int nBuffSize 버퍼의 사이즈 */
int FAS_GetSlaveInfoEx(int iBdID, BYTE pType, LPSTR LpBuff, int nBuffSize){
    buffer[0] = header; buffer[1] = 0x03; buffer[2] = sync_no; buffer[3] = 0x00; buffer[4] = frame_type;
    return 0;
}

 /**@brief 현재까지 수정된 파라미터 값고 입출력 신호를 ROM영역에 저장
  * @param int iBdID 드라이브 ID*/
int FAS_SaveAllParameters(int iBdID){
    buffer[0] = header; buffer[1] = 0x03; buffer[2] = sync_no; buffer[3] = 0x00; buffer[4] = frame_type;
    return 0;
}

 /**@brief 비상정지
  * @param int iBdID 드라이브 ID
  * @return 명령이 수행된 정보*/
int FAS_EmergencyStop(int iBdID){
    buffer[0] = header; buffer[1] = 0x03; buffer[2] = sync_no; buffer[3] = 0x00; buffer[4] = frame_type;
    return 0;
}

 /**@brief Servo의 상태를 ON/OFF
  * @param int iBdID 드라이브 ID 
  * @param bool bOnOff Enable/Disable
  * @return 명령이 수행된 정보*/
int FAS_ServoEnable(int iBdID, bool bOnOff){
    buffer[0] = header; buffer[1] = 0x04; buffer[2] = sync_no; buffer[3] = 0x00; buffer[4] = frame_type; buffer[5] = data[0];
    return 0;
}

 /**@brief Alarm Reset명령 보냄
  * @param int iBdID 드라이브 ID*/
int FAS_ServoAlarmReset(int iBdID){
    buffer[0] = header; buffer[1] = 0x03; buffer[2] = sync_no; buffer[3] = 0x00; buffer[4] = frame_type;
    return 0;
}

 /**@brief Alarm 정보 요청
  * @param int iBdID 드라이브 ID*/
int FAS_GetAlarmType(int iBdID){
    buffer[0] = header; buffer[1] = 0x03; buffer[2] = sync_no; buffer[3] = 0x00; buffer[4] = frame_type;
    return 0;
}

 /**@brief Servo를 천천히 멈추는 기능
  * @param int iBdID 드라이브 ID
  * @return 명령이 수행된 정보*/
int FAS_MoveStop(int iBdID){
    buffer[0] = header; buffer[1] = 0x03; buffer[2] = sync_no; buffer[3] = 0x00; buffer[4] = frame_type;
    return 0;
}

 /**@brief 시스템의 원점을 찾는 기능?
  * @param int iBdID 드라이브 ID
  * @return 명령이 수행된 정보*/
int FAS_MoveOriginSingleAxis(int iBdID){
    buffer[0] = header; buffer[1] = 0x03; buffer[2] = sync_no; buffer[3] = 0x00; buffer[4] = frame_type;
    return 0;
}

/**@brief Jog 운전 시작을 요청
  * @param int iBdID 드라이브 ID
  * @param DWORD lVelocity 이동 시 속도 값 (pps)
  * @param int iVelDir 이동할 방향 (0:-Jog, 1:+Jog)
  * @return 명령이 수행된 정보*/
int FAS_MoveVelocity(int iBdID, DWORD lVelocity, int iVelDir) {
    buffer[0] = header; buffer[1] = 0x08; buffer[2] = sync_no; buffer[3] = 0x00; buffer[4] = frame_type;
    memcpy(&buffer[5], data, sizeof(data));
    return 0;
}
/************************************************************************************************************************************
 ******************************************************* 편의상 만든 함수 **************************************************************
 ************************************************************************************************************************************/
 
 /**@brief 명령전달에 쓰는 버퍼 내용을 터미널에 일단 보여주는 함수*/
 void print_buffer(uint8_t *array, size_t size) {
    for (size_t i = 0; i < size; i++) {
        printf("%02X ", array[i]);
    }
    printf("\n");
}

/**@brief  배열을 문자열로 변환하는 함수*/
char* array_to_string(const uint8_t *array, int size) {
    char *str = g_strdup_printf("%02X", array[0]); // 첫 번째 요소는 따로 처리

    char *temp;
    for (int i = 1; i < size; i++) { // 첫 번째 요소 이후의 요소들 처리
        temp = g_strdup_printf("%s %02X", str, array[i]);
        g_free(str);
        str = temp;
    }

    return str;
}

 /**@brief 함수들을 찾아가게하는 인터페이스 용도 함수*/
void library_interface(){
    switch(frame_type)
    {
        case 0x01:
            FAS_GetboardInfo(0, 0, NULL, 0);
            break;
        case 0x05:
            FAS_GetMotorInfo(0, 0, NULL, 0);
            break;
        case 0x06:
            FAS_GetEncoder(0, 0, NULL, 0);
            break;
        case 0x07:
            FAS_GetFirmwareInfo(0, 0, NULL, 0);
            break;
        case 0x09:
            FAS_ServoEnable(0, 0);
            break;
        case 0x10:
            FAS_SaveAllParameters(0);
            break;
        case 0x2A:
            FAS_ServoEnable(0, 0);
            break;
        case 0x2B:
            FAS_ServoAlarmReset(0);
            break;
        case 0x2E:
            FAS_GetAlarmType(0);
            break;
        case 0x31:
            FAS_MoveStop(0);
            break;
        case 0x32:
            FAS_EmergencyStop(0);
            break;
        case 0x33:
            FAS_MoveOriginSingleAxis(0);
            break;
         case 0x37:
            FAS_MoveVelocity(0, 1000, 0); // 예시로 lVelocity를 1000, iVelDir를 0으로 설정
            break;
    }
    size_t data_size = buffer[1] + 2;
    print_buffer(buffer, data_size);
    
    char *text = array_to_string(buffer, buffer[1] + 2);
    gtk_text_buffer_set_text(sendbuffer_buffer, text, -1);
}

 /**@brief 각 명령어의 함수 이름을 찾아가는 인터페이스 용도 함수*/
char *command_interface(){
    switch(frame_type)
    {
        case 0x01:
            return"FAS_GetboardInfo";
        case 0x05:
            return "FAS_GetMotorInfo";
        case 0x06:
            return "FAS_GetEncoder";
        case 0x07:
            return "FAS_GetFirmwareInfo";
        case 0x09:
            return "FAS_ServoEnable";
        case 0x10:
            return "FAS_SaveAllParameters";
        case 0x2A:
            return "FAS_ServoEnable";
        case 0x2B:
            return "FAS_ServoAlarmReset";
        case 0x2E:
            return "FAS_GetAlarmType";
        case 0x31:
            return "FAS_MoveStop";
        case 0x32:
            return "FAS_EmergencyStop";
        case 0x33:
            return "FAS_MoveOriginSingleAxis";
         case 0x37:
            return "FAS_MoveVelocity";
        default:
            return "Transfer Fail";
    }
}

 /**@brief 통신상태에서 출력할 내용을 찾아가는 인터페이스 용도 함수*/
char* FMM_interface(FMM_ERROR error) {
    switch (error) {
        case FMM_OK:
            return "FMM_OK";
        case FMM_NOT_OPEN:
            return "FMM_NOT_OPEN";
        case FMM_INVALID_PORT_NUM:
            return "FMM_INVALID_PORT_NUM";
        case FMM_INVALID_SLAVE_NUM:
            return "FMM_INVALID_SLAVE_NUM";
        case FMC_DISCONNECTED:
            return "FMC_DISCONNECTED";
        case FMC_TIMEOUT_ERROR:
            return "FMC_TIMEOUT_ERROR";
        case FMC_CRCFAILED_ERROR:
            return "FMC_CRCFAILED_ERROR";
        case FMC_RECVPACKET_ERROR:
            return "FMC_RECVPACKET_ERROR";
        case FMM_POSTABLE_ERROR:
            return "FMM_POSTABLE_ERROR";
        case FMP_FRAMETYPEERROR:
            return "FMP_FRAMETYPEERROR";
        case FMP_DATAERROR:
            return "FMP_DATAERROR";
        case FMP_PACKETERROR:
            return "FMP_PACKETERROR";
        case FMP_RUNFAIL:
            return "FMP_RUNFAIL";
        case FMP_RESETFAIL:
            return "FMP_RESETFAIL";
        case FMP_SERVOONFAIL1:
            return "FMP_SERVOONFAIL1";
        case FMP_SERVOONFAIL2:
            return "FMP_SERVOONFAIL2";
        case FMP_SERVOONFAIL3:
            return "FMP_SERVOONFAIL3";
        case FMP_SERVOOFF_FAIL:
            return "FMP_SERVOOFF_FAIL";
        case FMP_ROMACCESS:
            return "FMP_ROMACCESS";
        case FMP_PACKETCRCERROR:
            return "FMP_PACKETCRCERROR";
        case FMM_UNKNOWN_ERROR:
            return "FMM_UNKNOWN_ERROR";
        default:
            return "Unknown error";
    }
}

void syno_no_update(){
    sync_no++;
    char sync_str[4];
    sprintf(sync_str, "%u", sync_no);
    gtk_text_buffer_set_text(autosync_buffer, sync_str, -1);
}

char* get_time() {
    time_t currentTime;
    struct tm* timeInfo;
    char* timeString = (char*)malloc(9); // "hh:mm:ss" + null terminator

    if (timeString == NULL) {
        printf("메모리 할당 오류\n");
        return NULL;
    }

    // 시스템의 현재 시간 가져오기
    currentTime = time(NULL);

    // 현재 시간을 localtime 함수를 이용하여 시간 구조체에 저장
    timeInfo = localtime(&currentTime);

    // 시간:분:초 형식으로 문자열로 변환
    strftime(timeString, 9, "%H:%M:%S", timeInfo);
    return timeString;
}

void handle_alarm(int signum) {
    // 타이머 시간이 초과되면 SIGALRM 신호가 발생합니다.
    // 이때 소켓을 닫고 연결 실패로 간주합니다.
    perror("Connection timed out");
    close(client_socket);
    
    gtk_label_set_text(label_status, "NG");
}

void send_packet(BYTE *byte_array){
    
    syno_no_update();
    char* currentTimeString = get_time();
    if (currentTimeString != NULL) {
        gtk_label_set_text(label_time, currentTimeString);
        free(currentTimeString); // 메모리 해제
    }
    gtk_label_set_text(label_status, "Sending");
    
    if(show){
        char *text = array_to_string(byte_array, byte_array[1] + 2);
        gtk_text_buffer_set_text(monitor1_buffer, text, -1);
        
        frame_type = byte_array[4];
        char *command = command_interface();
        gtk_text_buffer_set_text(monitor2_buffer, "[SEND]", -1);
        
        GtkTextIter iter;
        gtk_text_buffer_get_end_iter(monitor2_buffer, &iter);
        gtk_text_buffer_insert(monitor2_buffer, &iter, "\n", -1);
        gtk_text_buffer_insert(monitor2_buffer, &iter, command, -1);
        gtk_text_buffer_insert(monitor2_buffer, &iter, "\n", -1);
        gtk_text_buffer_insert(monitor2_buffer, &iter, "\n", -1);
    }
    int send_result = sendto(client_socket, byte_array, byte_array[1] + 2, 0, (const struct sockaddr *)&server_addr, sizeof(server_addr));
    if (send_result < 0) {
        perror("sendto failed");
        return;
    }
    alarm(TIMEOUT_SECONDS);
    while(1){
        ssize_t received_bytes = recvfrom(client_socket, buffer, sizeof(buffer), 0, NULL, NULL);
        
        if (received_bytes < 0) {
            perror("recvfrom failed");
            break;
        }

        // Print the received data in hexadecimal format
        printf("Server: ");
        for (ssize_t i = 0; i < received_bytes; i++) {
            printf("%02x ", (BYTE)buffer[i]);
        }
        printf("\n");
        
        gtk_label_set_text(label_status, "OK");
        alarm(0);
        if(show){
            FMM_ERROR errorCode = buffer[5];
            char *errorMsg = FMM_interface(errorCode);
            
            char *response_text = array_to_string(buffer, received_bytes);
            GtkTextIter iter;
            gtk_text_buffer_get_end_iter(monitor1_buffer, &iter);
            gtk_text_buffer_insert(monitor1_buffer, &iter, "\n", -1); // Add a newline
            gtk_text_buffer_insert(monitor1_buffer, &iter, "\n", -1); // Add a newline
            gtk_text_buffer_insert(monitor1_buffer, &iter, response_text, -1);
            
            frame_type = buffer[4];
            char *command = command_interface();
            gtk_text_buffer_get_end_iter(monitor2_buffer, &iter);
            gtk_text_buffer_insert(monitor2_buffer, &iter, "[RECEIVE]", -1);
            gtk_text_buffer_insert(monitor2_buffer, &iter, "\n", -1);
            gtk_text_buffer_insert(monitor2_buffer, &iter, command, -1);
            gtk_text_buffer_insert(monitor2_buffer, &iter, "\n", -1);
            gtk_text_buffer_insert(monitor2_buffer, &iter, "RESPONSE : ", -1);
            gtk_text_buffer_insert(monitor2_buffer, &iter, errorMsg, -1);
        }
        else{
            break;
        }
        break;
    }
    memset(&buffer, 0, sizeof(buffer));
}

void send_packetTCP(BYTE *byte_array){
    
    syno_no_update();
    char* currentTimeString = get_time();
    if (currentTimeString != NULL) {
        gtk_label_set_text(label_time, currentTimeString);
        free(currentTimeString); // 메모리 해제
    }
    gtk_label_set_text(label_status, "Sending");
    if(show){
        char *text = array_to_string(byte_array, byte_array[1] + 2);
        gtk_text_buffer_set_text(monitor1_buffer, text, -1);
        frame_type = byte_array[4];
        char *command = command_interface();
        gtk_text_buffer_set_text(monitor2_buffer, "[SEND]", -1);
        GtkTextIter iter;
        gtk_text_buffer_get_end_iter(monitor2_buffer, &iter);
        gtk_text_buffer_insert(monitor2_buffer, &iter, "\n", -1);
        gtk_text_buffer_insert(monitor2_buffer, &iter, command, -1);
        gtk_text_buffer_insert(monitor2_buffer, &iter, "\n", -1);
        gtk_text_buffer_insert(monitor2_buffer, &iter, "\n", -1);
    }
    ssize_t sent_bytes = send(client_socket, byte_array, byte_array[1] + 2, 0);
    if (sent_bytes < 0) {
        perror("sendto failed");
    }
    
    memset(&buffer, 0, sizeof(buffer));
    
    while (1) {
        // Receive message from server
        int received_bytes = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        buffer[received_bytes] = '\0';
        
        printf("Server: ");
        for (ssize_t i = 0; i < received_bytes; i++) {
            printf("%02x ", (BYTE)buffer[i]);
        }
        printf("\n");
        gtk_label_set_text(label_status, "OK");
        if(show){
            FMM_ERROR errorCode = buffer[5];
            char *errorMsg = FMM_interface(errorCode);
            
            char *response_text = array_to_string(buffer, received_bytes);
            GtkTextIter iter;
            gtk_text_buffer_get_end_iter(monitor1_buffer, &iter);
            gtk_text_buffer_insert(monitor1_buffer, &iter, "\n", -1); // Add a newline
            gtk_text_buffer_insert(monitor1_buffer, &iter, "\n", -1); // Add a newline
            gtk_text_buffer_insert(monitor1_buffer, &iter, response_text, -1);
            
            frame_type = buffer[4];
            char *command = command_interface();
            gtk_text_buffer_get_end_iter(monitor2_buffer, &iter);
            gtk_text_buffer_insert(monitor2_buffer, &iter, "[RECEIVE]", -1);
            gtk_text_buffer_insert(monitor2_buffer, &iter, "\n", -1);
            gtk_text_buffer_insert(monitor2_buffer, &iter, command, -1);
            gtk_text_buffer_insert(monitor2_buffer, &iter, "\n", -1);
            gtk_text_buffer_insert(monitor2_buffer, &iter, "RESPONSE : ", -1);
            gtk_text_buffer_insert(monitor2_buffer, &iter, errorMsg, -1);
        }
        else{
            break;
        }
        break;
    }
    memset(&buffer, 0, sizeof(buffer));
}
