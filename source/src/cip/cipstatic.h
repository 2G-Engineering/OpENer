/*
 * cipstatic.h
 *
 *  Created on: Jun 7, 2018
 *      Author: ctaglia
 */

#ifndef CIPSTATIC_H_
#define CIPSTATIC_H_

#include "ciptypes.h"
#include "cipcommon.h"
#include "cipidentity.h"

//identifier macros used to reference pieces of the class structure
#define META_CLASS_NAME_STR(class_name)      ("Meta-"class_name)
#define META_CLASS_NAME(class_name)          meta_ ## class_name
#define CLASS_INSTANCE_NAME(class_name)      class_name ## _instance
#define CLASS_ATTR_NAME(class_name)          class_name ## _attrs
#define CLASS_SVC_NAME(class_name)           class_name ## _services
#define META_CLASS_ATTR_COUNT(count)         (7)

//Struct builder macros
#define CIP_CLASS_INSTANCE(instance_number, attrs_ptr, cip_class_ptr) \
                          {instance_number, attrs_ptr, cip_class_ptr, NULL} /* the next link will always be NULL, since there is only one instance of any particular class object */

#define CIP_CLASS_STRUCT_FORWARD_DECLARE(class_name)    const CipClass class_name; //create a tentative definition for this object so the compiler can put it in the attributes struct

#define CIP_CLASS_STRUCT_DEF(class_name,\
                          instance_number,\
                          attrs_ptr,\
                          cip_class_ptr,\
                          class_id,\
                          revision,\
                          number_of_instances,\
                          number_of_class_attributes,\
                          highest_class_attribute_number,\
                          number_of_class_services,\
                          instances_ptr,\
                          services_ptr,\
                          name_str)\
                const CipClass class_name = { \
                    CIP_CLASS_INSTANCE(instance_number, (CipAttributeStruct *)attrs_ptr,  (CipClass *)cip_class_ptr), \
                    class_id,\
                    revision,\
                    number_of_instances,\
                    number_of_class_attributes,\
                    highest_class_attribute_number,\
                    number_of_class_services,\
                    (CipInstance *)instances_ptr,\
                    (CipServiceStruct *)services_ptr,\
                    name_str\
    \
};

#define CIP_CLASS_DEFAULT_ATTRS(class_name) {{1, kCipUint, kGetableSingleAndAll, (void *) &class_name.revision}, {2, kCipUint, kGetableSingleAndAll, (void *) &class_name.number_of_instances}, \
{3, kCipUint, kGetableSingleAndAll, (void *) &class_name.number_of_instances}, {4, kCipUint, kNotSetOrGetable, (void *) &kCipUintZero}, {5, kCipUint, kNotSetOrGetable, (void *) &kCipUintZero},\
{6, kCipUint, kGetableSingleAndAll, (void *) &META_CLASS_NAME(class_name).highest_attribute_number}, {7, kCipUint, kGetableSingleAndAll, (void *) &class_name.highest_attribute_number}, }

#define CIP_CLASS_DEFAULT_IDENTITY_ATTRS(class_name)

#define CIP_CLASS_ATTRS(class_name, attr_count)             const CipAttributeStruct CLASS_ATTR_NAME(class_name)[attr_count] = CIP_CLASS_DEFAULT_ATTRS(class_name);//fixme define attrs
#define CIP_CLASS_SERVICES(class_name, svc_count)           const CipServiceStruct CLASS_SVC_NAME(class_name)[svc_count] = {0};
#define CIP_CLASS_INSTANCES(class_name, instance_count, instance_attrs)     const CipInstance      CLASS_INSTANCE_NAME(class_name)[instance_count] = {{1, (CipAttributeStruct *)&instance_attrs[0], (CipClass *)&class_name, 0}};

//The class has a number_of_services field which should limit access to the getAll method if only one service is enabled.
static const CipServiceStruct std_metaclass_services[2] = {{kGetAttributeSingle, &GetAttributeSingle, "GetAttributeSingle"}, {kGetAttributeAll, &GetAttributeAll, "GetAttributeAll"}};
static const CipServiceStruct std_class_services[3] = {{kGetAttributeSingle, &GetAttributeSingle, "GetAttributeSingle"}, {kGetAttributeAll, &GetAttributeAll, "GetAttributeAll"}, {kReset, &IdnReset, "Reset"}};


#define STATIC_CIP_CLASS( class_name,\
                          class_id,\
                          number_of_class_attributes,\
                          highest_class_attribute_number,\
                          number_of_class_services,\
                          number_of_instance_attributes,\
                          highest_instance_attribute_number,\
                          number_of_instance_services,\
                          number_of_instances,\
                          name_str,\
                          revision,\
                          instance_attrs)\
                          \
  CIP_CLASS_STRUCT_FORWARD_DECLARE(class_name)\
  CIP_CLASS_STRUCT_FORWARD_DECLARE(META_CLASS_NAME(class_name))\
  CIP_CLASS_ATTRS(class_name, META_CLASS_ATTR_COUNT(number_of_class_attributes))\
  CIP_CLASS_INSTANCES(class_name, number_of_instances, instance_attrs)\
  CIP_CLASS_STRUCT_DEF(META_CLASS_NAME(class_name), /* Metaclass Definition*/\
  0xffffffff,      /*the metaclass object does not really have a valid instance number*/\
  0,               /* metaclass has no attributes*/\
  0,               /* the metaclass has no class*/\
  0xffffffff,      /* set metaclass ID (this should never be referenced) */\
  0,               /* No particular revision on the metaclass*/\
  1,               /* the class object is the only instance of the metaclass */\
  number_of_class_attributes,/* the metaclass remembers how many class attributes exist*/\
  highest_class_attribute_number,\
  number_of_class_services,\
  &class_name,\
  &std_metaclass_services[0],\
  META_CLASS_NAME_STR(name_str)\
  )\
  \
  CIP_CLASS_STRUCT_DEF(class_name, /*Actual Class Definition*/\
  0, \
  &CLASS_ATTR_NAME(class_name)[0],\
  &META_CLASS_NAME(class_name),\
  class_id,\
  revision,\
  number_of_instances,\
  number_of_instance_attributes,\
  highest_instance_attribute_number,\
  number_of_instance_services,\
  &CLASS_INSTANCE_NAME(class_name)[0],\
  &std_class_services[0],\
  name_str\
  )


#endif /* CIPSTATIC_H_ */
