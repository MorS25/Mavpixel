/*
///////////////////////////////////////////////////////////////////////
//
// Please read licensing, redistribution, modifying, authors and 
// version numbering from main sketch file. This file contains only
// a minimal header.
//
// Mavpixel Mavlink Neopixel bridge
// (c) 2016 Nick Metcalfe
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
//  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
//  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
//  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, 
//  INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, 
//  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, 
//  OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
//  WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
//  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
//  POSSIBILITY OF SUCH DAMAGE.
//
/////////////////////////////////////////////////////////////////////////////
*/
//
// Mavpixel definitions
// 4/6/2016

// Some basic defualts
#define EN  1     // Enable value
#define DI  0     // Disable value
#define TRUE 1    // Like we would not know what true is
#define FALSE 0   // or this too...

// Flight mode defines
#define STAB 0
#define ACRO 1
#define ALTH 2
#define AUTO 3
#define GUID 4
#define LOIT 5
#define RETL 6
#define CIRC 7
#define LAND 8
#define DRFT 9
#define SPRT 10
#define FLIP 11
#define ATUN 12
#define POSH 13
#define BRAK 14
#define THRO 15
//APM:plane
#define MANU 16
#define FBWA 17
#define FBWB 18
#define TRAN 19
#define CRUS 20

#define MAX_MODES 20

//Command/parameter string defs
const char PROGMEM cmd_version_P[] = "version";
const char PROGMEM cmd_sysid_P[] = "sysid";
const char PROGMEM cmd_quit_P[] = "quit";
const char PROGMEM cmd_led_P[] = "led";
const char PROGMEM cmd_color_P[] = "color";
const char PROGMEM cmd_baud_P[] = "baud";
const char PROGMEM cmd_soft_P[] = "softbaud";
const char PROGMEM cmd_free_P[] = "free";
const char PROGMEM cmd_vars_P[] = "vars";
const char PROGMEM cmd_mode_P[] = "mode_color";
const char PROGMEM cmd_lbv_P[] = "lowcell";
const char PROGMEM cmd_lbp_P[] = "lowpct";
const char PROGMEM cmd_bright_P[] = "brightness";
const char PROGMEM cmd_anim_P[] = "animation";
const char PROGMEM cmd_freset_P[] = "factory";
const char PROGMEM cmd_minsats_P[] = "minsats";
const char PROGMEM cmd_reboot_P[] = "reboot";
const char PROGMEM cmd_help_P[] = "help";
const char PROGMEM cmd_deadband_P[] = "deadband";
const char PROGMEM cmd_lamptest_P[] = "lamptest";
//For Mavlink parameter communications
const char PROGMEM mav_led_P[] = "led_";
const char PROGMEM mav_mode_P[] = "mode_";
const char PROGMEM mav_color_P[] = "color_";

// MAVLink HeartBeat bits
#define MOTORS_ARMED 128

#define ONBOARD_PARAM_COUNT 82
///////////////////////////
// Global variables

// Counters and millisecond placeholders used around the code
static uint32_t hbMillis;                         // HeartBeat timer
static uint32_t hbTimer = 500;

//Parameter send timer
static uint32_t parMillis;                         // Parameter timer
static uint32_t parTimer = 50;

//Led timer
static uint32_t led_flash = 500;
static uint32_t p_led;

static float    iob_vbat_A = 0;                 // Battery A voltage in milivolt
static uint16_t iob_battery_remaining_A = 0;    // 0 to 100 <=> 0 to 1000

static uint16_t iob_mode = 0;                   // Navigation mode from RC AC2 = CH5, APM = CH8
static uint16_t iob_old_mode = 0;
static uint8_t iob_state = 0;

static uint8_t  iob_satellites_visible = 0;     // number of satelites
static uint8_t  iob_fix_type = 0;               // GPS lock 0-1=no fix, 2=2D, 3=3D
static unsigned int iob_hdop=0;

static int16_t   iob_chan1 = 1500;              //Roll
static int16_t   iob_chan2 = 1500;              //Pitch
static uint16_t  iob_throttle = 0;               // throtle

//MAVLink session control
static uint8_t  apm_mav_type;
static uint8_t  apm_mav_system; 
static uint8_t  apm_mav_component;
static boolean  enable_mav_request = 0;
static int16_t  m_parameter_i = ONBOARD_PARAM_COUNT;

static uint8_t system_mode = MAV_MODE_PREFLIGHT; ///< Booting up
static uint32_t custom_mode = 0;                 ///< Custom mode, can be defined by user/adopter
static uint8_t system_state = MAV_STATE_STANDBY; ///< System ready for flight


char mavParamBuffer[MAVLINK_MSG_PARAM_SET_FIELD_PARAM_ID_LEN];

// General states
byte flMode;          // Our current flight mode as defined
byte isArmed = 0;     // Is motors armed flag
static uint8_t crlf_count = 0;
uint8_t mySysId;

//LED Strip vars
#ifdef LED_STRIP
uint8_t lowBattPct;
float lowBattVolt;
uint8_t numCells = 0;
float cellVoltage = 0;
uint8_t stripBright = 0;
boolean stripAnim;
uint8_t minSats;
uint8_t deadBand;
uint8_t lampTest;
#endif

