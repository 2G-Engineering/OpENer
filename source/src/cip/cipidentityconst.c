/*
 * cipidentityconst.c
 *
 *  Created on: Jun 7, 2018
 *      Author: jlange
 */

#include "cipstatic.h"
#include "cipassembly.h"
#include "cipidentity.h"
#include "cipmessagerouter.h"
#include "opener_user_conf.h"

#ifdef OPENER_CIP_CONST_IDENTITY

const EipUint16 vendor_id_ = OPENER_DEVICE_VENDOR_ID; /**< Attribute 1: Vendor ID */
const EipUint16 device_type_ = OPENER_DEVICE_TYPE; /**< Attribute 2: Device Type */
const EipUint16 product_code_ = OPENER_DEVICE_PRODUCT_CODE; /**< Attribute 3: Product Code */
const CipRevision revision_ = { OPENER_DEVICE_MAJOR_REVISION,
                          OPENER_DEVICE_MINOR_REVISION }; /**< Attribute 4: Revision / USINT Major, USINT Minor */
const CipShortString product_name_ = { sizeof(OPENER_DEVICE_NAME) - 1,
                                 (EipByte*)OPENER_DEVICE_NAME }; /**< Attribute 7: Product Name */

extern EipUint16 status_;
extern EipUint32 serial_number_;

const CipAttributeStruct identity_attrs[7] = {{1, kCipUint, kGetableSingleAndAll, (void *) &vendor_id_}, {2, kCipUint, kGetableSingleAndAll, (void *) &device_type_}, \
    {3, kCipUint, kGetableSingleAndAll, (void *) &product_code_}, {4, kCipUsintUsint, kGetableSingleAndAll, (void *) &revision_}, {5, kCipWord, kGetableSingleAndAll, (void *) &status_},\
    {6, kCipUdint, kGetableSingleAndAll, (void *) &serial_number_}, {7, kCipShortString, kGetableSingleAndAll, (void *) &product_name_} };

    STATIC_CIP_CLASS(static_identity_object, /* class name */
        kIdentityClassCodeDef, /* class ID */
        7, /* # class attributes*/
        7, /* # highest class attribute number*/
        2, /* # class services*/
        7, /* # instance attributes*/
        7, /* # highest instance attribute number*/
        3, /* # instance services*/
        1, /* # instances*/
        "identity", /* name */
        1,/* Revision, according to the CIP spec currently this has to be 2 */
        identity_attrs);

    EipStatus CipIdentityInit() {
      RegisterCipClass((CipClass*)&static_identity_object);
      return kEipStatusOk;
    }
#endif
