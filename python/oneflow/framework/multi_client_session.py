"""
Copyright 2020 The OneFlow Authors. All rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
"""
import enum
import inspect

from google.protobuf import text_format

import oneflow._oneflow_internal
import oneflow.core.job.job_set_pb2 as job_set_util
import oneflow.framework.c_api_util as c_api_util


class MultiClientSession(object):
    class Status(enum.Enum):
        CREATED = 1
        INITED = 2
        CLOSED = 3

    def __init__(self, sess_id, env_holder):
        self._env_holder = env_holder
        self.sess_ = oneflow._oneflow_internal.RegsiterSession(sess_id)
        oneflow._oneflow_internal.CreateMultiClientSessionContext()
        self.config_proto_ = self._make_config_proto()
        self.function_flag_name2default_val_ = {}
        self._update_function_flag_name2defaultVal()
        self.scope_attr_name2default_val_ = {}
        self._update_scope_attr_name2defaultVal()
        self.status_ = self.Status.CREATED

    def TryInit(self):
        self._check_status(self.Status.CREATED, self.Status.INITED)
        if self.status_ == self.Status.CREATED:
            config_proto_str = text_format.MessageToString(self.config_proto)
            oneflow._oneflow_internal.InitMultiClientSessionContext(config_proto_str)
            self.status_ = self.Status.INITED

    def _TryClose(self):
        if self.status_ != self.Status.CLOSED:
            oneflow._oneflow_internal.TryDestroyMultiClientSessionContext()
            oneflow._oneflow_internal.ClearSessionById(self.id)
        self.status_ = self.Status.CLOSED

    def AddCGraph(self, graph):
        self._check_status(self.Status.INITED)
        oneflow._oneflow_internal.MultiClientSessionContextAddCGraph(graph)

    @property
    def status(self):
        return self.status_

    @property
    def id(self):
        return self.sess_.id

    @property
    def config_proto(self):
        return self.config_proto_

    @property
    def resource(self):
        self._check_status(self.Status.INITED)
        return c_api_util.CurrentResource()

    @property
    def function_flag_name2default_val(self):
        return self.function_flag_name2default_val_

    @property
    def scope_attr_name2default_val(self):
        return self.scope_attr_name2default_val_

    @property
    def is_running(self):
        return self.status_ == self.Status.INITED

    def AnyGlobalFunctionDefined(self):
        return False

    def _check_status(self, *status):
        check_success = False
        for stat in status:
            if self.status_ == stat:
                check_success = True
                break
        if check_success is False:
            caller_func_name = inspect.stack()[1].function
            allowed_status = " or ".join([str(stat) for stat in status])
            raise ValueError(
                "The calling to {} is only allowed when status is {}, but current status is {}".format(
                    caller_func_name, allowed_status, self.status_
                )
            )

    def _make_config_proto(self):
        config_proto = job_set_util.ConfigProto()
        config_proto.resource.SetInParent()
        config_proto.session_id = self.id
        return config_proto

    def _update_function_flag_name2defaultVal(self):
        items = c_api_util.GetFunctionConfigDef().attr_name2attr_def.items()
        self.function_flag_name2default_val_ = {k: v.default_val for (k, v) in items}

    def _update_scope_attr_name2defaultVal(self):
        items = c_api_util.GetScopeConfigDef().attr_name2attr_def.items()
        self.scope_attr_name2default_val_ = {k: v.default_val for (k, v) in items}

    def update_resource_eagerly(self, resource_config):
        self._check_status(self.Status.INITED)
        config_proto_str = text_format.MessageToString(resource_config)
        oneflow._oneflow_internal.MultiClientSessionContextUpdateResource(
            config_proto_str
        )

    def __del__(self):
        self._TryClose()
        print("oneflow session del")

