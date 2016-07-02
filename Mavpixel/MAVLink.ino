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

#define MAVLINK_COMM_NUM_BUFFERS 1
#define MAVLINK_USE_CONVENIENCE_FUNCTIONS

// this code was moved from libraries/GCS_MAVLink to allow compile
// time selection of MAVLink 1.0
BetterStream	*mavlink_comm_0_port;
BetterStream	*mavlink_comm_1_port;

uint8_t system_type = 18;//MAV_TYPE_GENERIC;    //No type for a lighting controller, use next free?
//uint8_t autopilot_type = MAV_AUTOPILOT_GENERIC;
uint8_t autopilot_type = MAV_AUTOPILOT_INVALID;
mavlink_param_union_t param;
 
#include "Mavlink_compat.h"

//#ifdef MAVLINK10
#include "../GCS_MAVLink/include/mavlink/v1.0/mavlink_types.h"
#include "../GCS_MAVLink/include/mavlink/v1.0/ardupilotmega/mavlink.h"

//#else
//#include "../GCS_MAVLink/include/mavlink/v0.9/mavlink_types.h"
//#include "../GCS_MAVLink/include/mavlink/v0.9/ardupilotmega/mavlink.h"
//#endif

void HeartBeat() {
  uint32_t timer = millis();
  if(timer - hbMillis > hbTimer) 
  {
    hbMillis += hbTimer;
#ifdef HEARTBEAT
#ifndef SOFTSER  //Active CLI on telemetry port pauses mavlink
    if (!cli_active && heartBeat) {
#else
    if (heartBeat) {
#endif
      //emit heartbeat
      mavlink_message_t msg;
      mavlink_msg_heartbeat_send(MAVLINK_COMM_0, system_type, autopilot_type, system_mode, custom_mode, system_state);
    }
#endif 
#ifdef RATEREQ
    mavStreamRequest();  //Request rate control.
#endif
    messageCounter++;    //used to timeout Mavlink in main loop
  }
}

#ifdef RATEREQ
//Request data streams from flight controller if required
// periodically called from heartbeat
void mavStreamRequest() {
  //Do rate control requests
  if(enable_mav_request > 0) { //Request rate control.
    enable_mav_request--;
    request_mavlink_rates();   
  }
}
#endif

//Send Mavlink parameter list
// Called from main loop
void mavSendData() {
  uint32_t timer = millis();
  if(timer - parMillis > parTimer) {
    parMillis += parTimer;
    //send parameters one by one
    if (m_parameter_i < ONBOARD_PARAM_COUNT) {
      mavSendParameter(m_parameter_i);
      m_parameter_i++;
    }
  }
}  


#ifdef RATEREQ
void request_mavlink_rates()
{
  const int  maxStreams = 7;
  const uint8_t MAVStreams[maxStreams] = {MAV_DATA_STREAM_RAW_SENSORS,
					  MAV_DATA_STREAM_EXTENDED_STATUS,
                                          MAV_DATA_STREAM_RC_CHANNELS,
					  MAV_DATA_STREAM_POSITION,
                                          MAV_DATA_STREAM_EXTRA1, 
                                          MAV_DATA_STREAM_EXTRA2,
                                          MAV_DATA_STREAM_EXTRA3};
                                          
  const uint16_t MAVRates[maxStreams] = {0x02, 0x02, 0x05, 0x02, 0x05, 0x02, 0x02};

  for (int i=0; i < maxStreams; i++) {
    	  mavlink_msg_request_data_stream_send(MAVLINK_COMM_0,
					       apm_mav_system, apm_mav_component,
					       MAVStreams[i], MAVRates[i], 1);
  }
}
#endif

#ifdef SOFTSER
void read_softser(){
  while(dbSerial.available() > 0) { 
    uint8_t c = dbSerial.read();
    //Look for CLI on SoftSerial channel
    if (countCrLf(c)) return;
    if (cli_active) {
      CLIchar(c);
    }
  }
}
#endif

//Main mavlink reader
void read_mavlink(){
  mavlink_message_t msg; 
  mavlink_status_t status;
  
  // grabing data 
  while(Serial.available() > 0) { 
    uint8_t c = Serial.read();
#ifndef SOFTSER
    //Look for CLI on mavlink channel
    if (countCrLf(c)) return;
    if (cli_active) CLIchar(c);
    else {
#endif    
      // trying to grab msg  
      if(mavlink_parse_char(MAVLINK_COMM_0, c, &msg, &status)) {
         mavlink_active = 1;
         messageCounter = 0;
         led_flash = 500;

        // handle msg  
        switch(msg.msgid) {
          case MAVLINK_MSG_ID_HEARTBEAT:
            {
#ifdef DEBUG
              println(F("MAVLink HeartBeat"));
#endif
  	      apm_mav_system    = msg.sysid;
  	      apm_mav_component = msg.compid;
              apm_mav_type      = mavlink_msg_heartbeat_get_type(&msg);
  
              //Flight mode
              iob_mode = mavlink_msg_heartbeat_get_custom_mode(&msg);
              if(iob_mode != iob_old_mode) {
                iob_old_mode = iob_mode;
                CheckFlightMode();
              }                

              //Armed flag
              if(isBit(mavlink_msg_heartbeat_get_base_mode(&msg),MOTORS_ARMED)) {
                if(isArmed == 0) CheckFlightMode();
                isArmed = 1;  
              } 
              else isArmed = 0;
 
              //System status (starting up, failsafe)
              iob_state = mavlink_msg_heartbeat_get_system_status(&msg);
            }
            break;
          
          //Extended status stream (battery, GPS)
          case MAVLINK_MSG_ID_SYS_STATUS:
            {
              iob_vbat_A = (mavlink_msg_sys_status_get_voltage_battery(&msg) / 1000.0f);
              iob_battery_remaining_A = mavlink_msg_sys_status_get_battery_remaining(&msg);
  
#ifdef LED_STRIP
              //Detect number of cells
              if (numCells == 0 && iob_vbat_A > 7.5) {
                if(iob_vbat_A>21.2) numCells = 6;
                else if(iob_vbat_A>17) numCells = 5;
                else if(iob_vbat_A>12.8) numCells = 4;
                else if(iob_vbat_A>7.5) numCells = 3;
              }
              //Calculate cell voltage
              if (numCells > 0) cellVoltage = iob_vbat_A / numCells;
#endif

            }
            break;
          
          //Extended status stream (battery, GPS)
#ifndef MAVLINK10 
          case MAVLINK_MSG_ID_GPS_RAW:
            {
              iob_fix_type = mavlink_msg_gps_raw_get_fix_type(&msg);
            }
            break;
          case MAVLINK_MSG_ID_GPS_STATUS:
            {
              iob_satellites_visible = mavlink_msg_gps_status_get_satellites_visible(&msg);
            }
            break;
#else
          case MAVLINK_MSG_ID_GPS_RAW_INT:
            { 
              iob_hdop=(mavlink_msg_gps_raw_int_get_eph(&msg)/100);
              iob_fix_type = mavlink_msg_gps_raw_int_get_fix_type(&msg);
              iob_satellites_visible = mavlink_msg_gps_raw_int_get_satellites_visible(&msg);
            }
            break;  
#endif          
          //RC channels stream
          case MAVLINK_MSG_ID_RC_CHANNELS_RAW:
            {
              iob_chan1 = mavlink_msg_rc_channels_raw_get_chan1_raw(&msg);
              iob_chan2 = mavlink_msg_rc_channels_raw_get_chan2_raw(&msg);
            }
            break;
          
          //Extra 2 stream
          case MAVLINK_MSG_ID_VFR_HUD:
            {
              iob_throttle = mavlink_msg_vfr_hud_get_throttle(&msg);
              if(iob_throttle > 100 && iob_throttle < 150) iob_throttle = 100; //Temporary fix for ArduPlane 2.28
              if(iob_throttle < 0 || iob_throttle > 150) iob_throttle = 0; //Temporary fix for ArduPlane 2.28
            }
            break;
            
          case MAVLINK_MSG_ID_STATUSTEXT:
            {   
#ifdef DEBUG
             println(mavlink_msg_statustext_get_severity(&msg));
             println((char*)(&_MAV_PAYLOAD(&msg)[1]));            //print directly from mavlink buffer            
#endif
            }  
            break;
          case MAVLINK_MSG_ID_PARAM_REQUEST_LIST:
	    {
              mavlink_param_request_list_t request;
	      mavlink_msg_param_request_list_decode(&msg, &request);
	      // Check if this message is for this system
	      if (request.target_system == mavlink_system.sysid && request.target_component == mavlink_system.compid)
		// Start sending parameters
		m_parameter_i = 0;
	    }
	    break;
          case MAVLINK_MSG_ID_PARAM_REQUEST_READ:
	    {
              mavlink_param_request_read_t request;
	      mavlink_msg_param_request_read_decode(&msg, &request);
	      // Check if this message is for this system
	      if (request.target_system == mavlink_system.sysid && request.target_component == mavlink_system.compid)
                //Send single parameter (named parameters not supported, and not required by Mission Planner)
                mavSendParameter(mavlink_msg_param_request_read_get_param_index(&msg));
                //mavlink_msg_param_request_read_get_param_id(&msg, mavParamBuffer);
	    }
	    break;
          case MAVLINK_MSG_ID_MISSION_REQUEST_LIST:
	    {
              mavlink_mission_request_list_t request;
	      mavlink_msg_mission_request_list_decode(&msg, &request);
	      // Check if this message is for this system
	      if (request.target_system == mavlink_system.sysid && request.target_component == mavlink_system.compid)
                //Tell QGroundControl there is no mission
                mavlink_msg_mission_count_send(MAVLINK_COMM_0,
                  apm_mav_system,
                  apm_mav_component,
                  0);
	    }
	    break;
          case MAVLINK_MSG_ID_PARAM_SET:
	    {
              mavlink_param_set_t set;
	      mavlink_msg_param_set_decode(&msg, &set);
 
	      // Check if this message is for this system
	      if (set.target_system == mavlink_system.sysid && set.target_component == mavlink_system.compid)
              {
                uint8_t index = 0;
                boolean reboot = false;
                //Standard parameters
                if (strncmp_P(set.param_id, cmd_sysid_P, 5) == 0) {setSysid(set.param_value); index = 1;}
                if (strncmp_P(set.param_id, cmd_heart_P, 9) == 0) {setHeartbeat(set.param_value); index = 2;}
#ifdef LED_STRIP                
                else if (strncmp_P(set.param_id, cmd_bright_P, 10) == 0) {setBrightPct(set.param_value); index = 3;}
                else if (strncmp_P(set.param_id, cmd_anim_P, 9) == 0) {index = 4;
#ifdef USE_LED_ANIMATION
                  setStripAnim(set.param_value); 
#endif
                }
                else if (strncmp_P(set.param_id, cmd_lbv_P, 7) == 0) {setLowBattVolt(set.param_value); index = 5;}
                else if (strncmp_P(set.param_id, cmd_lbp_P, 6) == 0) {setLowBattPct(set.param_value); index = 6;}
                else if (strncmp_P(set.param_id, cmd_minsats_P, 7) == 0) {setMinSats(set.param_value); index = 7;}
                else if (strncmp_P(set.param_id, cmd_deadband_P, 8) == 0) {setDeadband(set.param_value); index = 8;}
                else if (strncmp_P(set.param_id, cmd_lamptest_P, 8) == 0) {lampTest = set.param_value; index = 11;}
#else
                else if (strncmp_P(set.param_id, cmd_bright_P, 10) == 0) index = 3;
                else if (strncmp_P(set.param_id, cmd_anim_P, 9) == 0) index = 4;
                else if (strncmp_P(set.param_id, cmd_lbv_P, 7) == 0) index = 5;
                else if (strncmp_P(set.param_id, cmd_lbp_P, 6) == 0) index = 6;
                else if (strncmp_P(set.param_id, cmd_minsats_P, 7) == 0) index = 7;
                else if (strncmp_P(set.param_id, cmd_deadband_P, 8) == 0) index = 8;
                else if (strncmp_P(set.param_id, cmd_lamptest_P, 8) == 0) index = 11;
#endif
                else if (strncmp_P(set.param_id, cmd_baud_P, 4) == 0) {setBaud(set.param_value); index = 9;}
                else if (strncmp_P(set.param_id, cmd_soft_P, 8) == 0) {index = 10;
#ifdef SOFSER
                  setSoftbaud(set.param_value); 
#endif
                }
                else if (strncmp_P(set.param_id, cmd_freset_P, 7) == 0) {writeEEPROM(FACTORY_RESET, 1); index = 12;}
                else if (strncmp_P(set.param_id, cmd_reboot_P, 6) == 0) {reboot = true; index = 13;}
                //LED parameters
                else if (strncmp_P(set.param_id, mav_led_P, 4) == 0) {
                  char* arg = strstr(cmdBuffer, "_");
                  if (arg > 0) {
                    index = atoi(arg + 1);
#ifdef LED_STRIP
                    memcpy(&ledConfigs[index], &param, 4);
                    writeLedConfig(index, &ledConfigs[index]);
#endif
                    index += 14;
                  }
                }
                //Mode parameters
                else if (strncmp_P(set.param_id, mav_mode_P, 5) == 0) {
                  char* arg = strstr(cmdBuffer, "_");
                  if (arg > 0) {
                    index = atoi(arg + 1);
#ifdef LED_STRIP
                    param.param_float = set.param_value;
                    writeModeColor(index, 0, param.bytes[0] & 0b00011111);
                    writeModeColor(index, 1, ((param.bytes[0] & 0b11100000) >> 5) + ((param.bytes[1] & 0b11100000) >> 2));
                    writeModeColor(index, 2, param.bytes[1] & 0b00011111);
                    writeModeColor(index, 3, param.bytes[2] & 0b00011111);
                    writeModeColor(index, 4, ((param.bytes[2] & 0b11100000) >> 5) + ((param.bytes[3] & 0b11100000) >> 2));
                    writeModeColor(index, 5, param.bytes[3] & 0b00011111);
#endif
                    index += 46;
                  }
                }
                //Color parameters
                else if (strncmp_P(set.param_id, mav_color_P, 6) == 0) {
                  char* arg = strstr(cmdBuffer, "_");
                  if (arg > 0) {
                    index = atoi(arg + 1);
#ifdef LED_STRIP
                    memcpy(colors[index], &param, 4);
                    writeColorConfig(index, colors[index]);
#endif
                    index += 67;
                  }
                }
                mavSendParameter(index);
                if (reboot) softwareReboot();
              }
            }
            break;
          case MAVLINK_MSG_ID_PING:
            {
              // process ping requests (tgt_system and tgt_comp must be zero)
              mavlink_ping_t ping;
              mavlink_msg_ping_decode(&msg, &ping);
              if(ping.target_system == 0 && (ping.target_component == 0 || ping.target_component == mavlink_system.compid))
                mavlink_msg_ping_send(MAVLINK_COMM_0, ping.time_usec, ping.seq, msg.sysid, msg.compid);
            }
            break;
          default:
            //Do nothing
            break;
        }
      }
#ifndef SOFTSER
    }
#endif    
    delayMicroseconds(138);
    //next one
  }
}

//Send a single Mavlink parameter by index
void mavSendParameter(int16_t index) {
  //Basic single-value parameters
  if (index == 0) mavlinkSendParam(cmd_version_P, -1, index, mavpixelVersion);
  else if (index == 1) mavlinkSendParam(cmd_sysid_P, -1, index, mavlink_system.sysid);
  else if (index == 2) mavlinkSendParam(cmd_heart_P, -1, index, heartBeat);
#ifdef LED_STRIP
  else if (index == 3) mavlinkSendParam(cmd_bright_P, -1, index, (uint8_t)((float)stripBright / 2.55f));
#ifdef USE_LED_ANIMATION
  else if (index == 4) mavlinkSendParam(cmd_anim_P, -1, index, stripAnim);
#else
  else if (index == 4) mavlinkSendParam(cmd_anim_P, -1, index, 0);
#endif
  else if (index == 5) mavlinkSendParam(cmd_lbv_P, -1, index, lowBattVolt);
  else if (index == 6) mavlinkSendParam(cmd_lbp_P, -1, index, lowBattPct);
  else if (index == 7) mavlinkSendParam(cmd_minsats_P, -1, index, minSats);
  else if (index == 8) mavlinkSendParam(cmd_deadband_P, -1, index, deadBand);
  else if (index == 11) mavlinkSendParam(cmd_lamptest_P, -1, index, lampTest);
#else
  else if (index == 3) mavlinkSendParam(cmd_bright_P, -1, index, 0);
  else if (index == 4) mavlinkSendParam(cmd_anim_P, -1, index, 0);
  else if (index == 5) mavlinkSendParam(cmd_lbv_P, -1, index, 0);
  else if (index == 6) mavlinkSendParam(cmd_lbp_P, -1, index, 0);
  else if (index == 7) mavlinkSendParam(cmd_minsats_P, -1, index, 0);
  else if (index == 8) mavlinkSendParam(cmd_deadband_P, -1, index, 0);
  else if (index == 11) mavlinkSendParam(cmd_lamptest_P, -1, index, 0);
#endif
  else if (index == 9) mavlinkSendParam(cmd_baud_P, -1, index, (uint32_t)readEP16(MAVLINK_BAUD) * 10);
  else if (index == 10) mavlinkSendParam(cmd_soft_P, -1, index, (uint32_t)readEP16(SOFTSER_BAUD) * 10);
  else if (index == 12) mavlinkSendParam(cmd_freset_P, -1, index, readEEPROM(FACTORY_RESET));
  else if (index == 13) mavlinkSendParam(cmd_reboot_P, -1, index, 0);
  //LEDs - sent as 4 byte XY(1):COLOR(1):FLAGS(2)
  else if (index >= 14 && index < 46) 
  {
#ifdef LED_STRIP
    memcpy(&param, &ledConfigs[index - 14], 4);     
    mavlinkSendParam(mav_led_P, index - 14, index, param.param_float);
#else
    mavlinkSendParam(mav_led_P, index - 14, index, 0);
#endif
  }
  //Modes - sent as a packed 4 byte representation - limits maximum modes to 32 (5-bit)
  // Packing: All six 5-bit color indexes in 4 bytes - 
  // North&low3bitsofEast:South&high2bitsofEast:West&low3bitsofUp:Down&high2bitsofUp
  else if (index >= 46 && index < 67) {
    uint8_t color = index - 46;
#ifdef LED_STRIP
    uint16_t c = readModeColor(color, 1);
    param.bytes[0] = readModeColor(color, 0) + ((c << 5) & 0b11100000);
    param.bytes[1] = readModeColor(color, 2) + ((c << 2) & 0b11100000);
    c = readModeColor(color, 4);
    param.bytes[2] = readModeColor(color, 3) + ((c << 5) & 0b11100000);
    param.bytes[3] = readModeColor(color, 5) + ((c << 2) & 0b11100000);
    mavlinkSendParam(mav_mode_P, color, index, param.param_float);
#else
    mavlinkSendParam(mav_mode_P, color, index, 0);
#endif
  }
  //Colour palette - sent as 4 byte Hue(2):Sat(1):Val(1) 
  else if (index >= 67 && index < 83) {
#ifdef LED_STRIP
    memcpy(&param, colors[index - 67], 4);
    mavlinkSendParam(mav_color_P, index - 67, index, param.param_float);
#else
    mavlinkSendParam(mav_color_P, index - 67, index, 0);
#endif
  }
}

//Send a parameter given a name, index and value
// if provided a non-negative nameIndex, append _<index> to name and send as uint32 rather than float
void mavlinkSendParam(const prog_char name_P[], int nameIndex, int index, float value) {
     strcpy_P(mavParamBuffer, name_P);
     if (nameIndex >= 0) itoa(nameIndex, mavParamBuffer + strlen_P(name_P), 10);
     mavlink_msg_param_value_send(MAVLINK_COMM_0, mavParamBuffer,
	  value, nameIndex >= 0 ? MAVLINK_TYPE_UINT32_T : MAVLINK_TYPE_FLOAT,
          ONBOARD_PARAM_COUNT, index);
}


