// Copyright 2023, Brian Swetland <swetland@frotz.net>
// Licensed under the Apache License, Version 2.0.

#pragma once

#define DAP_Info 0x00 // BYTE(DI_*)
// Response LEN BYTES[LEN]

#define DI_Vendor_Name               0x01
#define DI_Product_Name              0x02
#define DI_Serial_Number             0x03
#define DI_Protocol_Version          0x04
#define DI_Target_Device_Vendor      0x05
#define DI_Target_Device_Name        0x06
#define DI_Target_Board_Vendor       0x07
#define DI_Target_Board_Name         0x08
#define DI_Product_Firmware_Version  0x09
#define DI_Capabilities              0xF0 // BYTE
#define DI_Test_Domain_Timer         0xF1 // DWORD
#define DI_UART_RX_Buffer_Size       0xFB // WORD
#define DI_UART_TX_Buffer_Size       0xFC // WORD
#define DI_SWO_Trace_Buffer_Size     0xFD // WORD
#define DI_Max_Packet_Count          0xFE // BYTE
#define DI_Max_Packet_Size           0xFF // SHORT

// Capabilities Reply 0x00 LEN I0 I1  
#define I0_SWD  0x01
#define I0_JTAG 0x02
#define I0_SWO_UART 0x04
#define I0_SWO_Manchester 0x08
#define I0_Atomic_Commands 0x10
#define I0_Test_Domain_Timer 0x20
#define I0_SWO_Streaming_Trace 0x40
#define I0_UART_Comm_Port 0x80

#define I1_USB_COM_Port 0x01


#define DAP_HostStatus 0x01 // BYTE(Type) BYTE(ZeroOffOneOn)
// set host status LEDs
#define HS_Type_Connected 0
#define HS_Type_Running 1

#define DAP_Connect 0x02 // BYTE(Port)
// Reponse BYTE(Port)

#define PORT_DEFAULT 0
#define PORT_SWD 1
#define PORT_JTAG 2

#define DAP_Disconnect 0x03

#define DAP_WriteABORT 0x08 // BYTE(Index) WORD(value)
// Write an abort request to the target

#define DAP_Delay 0x09 // SHORT(DelayMicros)

#define DAP_ResetTarget 0x0A 
// Response BYTE(Status) BYTE(Execute)
// Execute 1 = device specific reset sequence implemented

#define DAP_SWJ_Pins 0x10 // BYTE(PinOut) BYTE(PinSel) WORD(PinWaitMicros)
// Response BYTE(PinInput)
// Modify pins (PinOut) where selected (PinSel)

#define PIN_SWCLK  0x01
#define PIN_TCK    0x01
#define PIN_SWDIO  0x02
#define PIN_TMS    0x02
#define PIN_TDI    0x04
#define PIN_TDO    0x08
#define PIN_nTRST  0x20
#define PIN_nRESET 0x80

#define DAP_SWD_Configure 0x13 // BYTE(Config)
#define CFG_Turnaround_1    0x00
#define CFG_Turnaround_2    0x01
#define CFG_Turnaround_3    0x02
#define CFG_Turnaround_4    0x03
#define CFG_AlwaysDataPhase 0x04

#define DAP_SWD_Sequence 0x1D 
// First Byte is Count
// Then one Info byte (and, if output, data) per Count
// Data is LSB first, padded to byte boundary
#define SEQ_OUTPUT 0x00
#define SEQ_INPUT  0x80
// Response: BYTE(STATUS) DATA (LSB first)


#define DAP_TransferConfigure 0x04
// BYTE(IdleCycles) SHORT(WaitRetry) SHORT(MatchRetry)
// idle cycles - number of extra idle cycles after each transfer
// wait retry - max number of retries after WAIT response
// match retry - max number of retries on reads w/ value match

#define DAP_Transfer 0x05
// BYTE(Index) BYTE(Count) followed by Count instances of
// BYTE(XferReq) WORD(Value) WORD(MatchMask) WORD(ValueMatch)
#define XFER_DebugPort  0x00
#define XFER_AccessPort 0x01
#define XFER_Write      0x00
#define XFER_Read       0x02
#define XFER_Addr_00    0x00
#define XFER_Addr_04    0x04
#define XFER_Addr_08    0x08
#define XFER_Addr_0C    0x0C
#define XFER_ValueMatch 0x10
#define XFER_MatchMask  0x20
#define XFER_TimeStamp  0x80

#define RSP_ACK_MASK 0x07
#define RSP_ACK_OK   0x01
#define RSP_ACK_WAIT 0x02
#define RSP_ACK_FAULT 0x04
#define RSP_ProtocolError 0x08
#define RSP_ValueMismatch 0x10
// Reponse BYTE(Count) BYTE(Response) WORD(TimeStamp)? WORD(Data)*

#define DAP_TransferBlock 0x06
// BYTE(Index) SHORT(Count) BYTE(XferReq) WORD(Data)*
// XFER as above but not ValueMatch/MatchMask/TimeStamp
// Response SHORT(Count) BYTE(Response) WORD(Data)*

#define DAP_ExecuteCommands 0x7F
// BYTE(Count) Count x Commands
// Response BYTE(Count) Count x Responses

#define DAP_QueueCommands 0x7E
// as above but N packets can be sent
// first non-queue-commands packet triggers execution

