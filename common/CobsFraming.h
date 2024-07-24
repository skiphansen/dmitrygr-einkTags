/*
 *  Copyright (C) 2023  Skip Hansen
 * 
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms and conditions of the GNU General Public License,
 *  version 2, as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 *  more details.
 *
 */
#ifndef _SERIALFRAMING_H_
#define _SERIALFRAMING_H_

int SerialFrameIO_Init(uint8_t *RxBuf,int RxBufSize);
int SerialFrameIO_ParseByte(uint8_t RxByte);
void SerialFrameIO_SendMsg(uint8_t *Msg,int MsgLen);
void SerialFrameIO_SendCmd(uint8_t Cmd,uint8_t *Data,int DataLen);
void SerialFrameIO_SendResp(uint8_t Cmd,uint8_t Err,uint8_t *Data,int DataLen);
int SerialFrameIO_CalcBufLen(int MaxMsgLen);

// Provide provided by user, sends or queues a byte to be sent on serial line
void SerialFrameIO_SendByte(uint8_t Byte);

#endif   // _SERIALFRAMING_H_

