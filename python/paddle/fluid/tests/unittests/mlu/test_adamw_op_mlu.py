#  Copyright (c) 2022 PaddlePaddle Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import numpy as np
import unittest
import sys

sys.path.append("..")
from op_test import OpTest
import paddle
import paddle.fluid as fluid
import paddle.fluid.core as core
from test_adam_op import adamw_step

paddle.enable_static()
SEED = 2022


class TestAdamW(OpTest):
    def setUp(self):
        self.set_mlu()
        self.op_type = "adamw"
        param = np.random.uniform(-1, 1, (105, 102)).astype("float32")
        grad = np.random.uniform(-1, 1, (105, 102)).astype("float32")
        moment1 = np.random.uniform(-1, 1, (105, 102)).astype("float32")
        # The second moment is positive
        moment2 = np.random.random((105, 102)).astype("float32")

        learning_rate = 0.5
        beta1 = 0.78
        beta2 = 0.836
        epsilon = 1e-4
        beta1_pow = beta1**10
        beta2_pow = beta2**10

        self.inputs = {
            'Param': param,
            'Grad': grad,
            'Moment1': moment1,
            'Moment2': moment2,
            'LearningRate': np.array([learning_rate]).astype("float32"),
            'Beta1Pow': np.array([beta1_pow]).astype("float32"),
            'Beta2Pow': np.array([beta2_pow]).astype("float32"),
        }

        self.attrs = {
            'epsilon': epsilon,
            'beta1': beta1,
            'beta2': beta2,
            "coeff": 0.9,
            "with_decay": True,
        }

        param_out, moment1_out, moment2_out = adamw_step(
            self.inputs, self.attrs
        )

        self.outputs = {
            'Moment1Out': moment1_out,
            'Moment2Out': moment2_out,
            'ParamOut': param_out,
            'Beta1PowOut': np.array([beta1_pow]).astype("float32") * beta1,
            'Beta2PowOut': np.array([beta2_pow]).astype("float32") * beta2,
        }

    def set_mlu(self):
        self.__class__.use_mlu = True
        self.place = paddle.device.MLUPlace(0)

    def init_dtype(self):
        self.dtype = np.float32

    def test_check_output(self):
        self.check_output_with_place(self.place, atol=1e-5)


class TestAdamOpWithSkipUpdate(OpTest):
    def setUp(self):
        self.set_mlu()
        self.op_type = "adamw"
        param = np.random.uniform(-1, 1, (102, 105)).astype("float32")
        grad = np.random.uniform(-1, 1, (102, 105)).astype("float32")
        moment1 = np.random.uniform(-1, 1, (102, 105)).astype("float32")
        # The second moment is positive
        moment2 = np.random.random((102, 105)).astype("float32")

        learning_rate = 0.004
        beta1 = 0.78
        beta2 = 0.836
        epsilon = 1e-4
        beta1_pow = beta1**10
        beta2_pow = beta2**10

        self.inputs = {
            'Param': param,
            'Grad': grad,
            'Moment1': moment1,
            'Moment2': moment2,
            'LearningRate': np.array([learning_rate]).astype("float32"),
            'Beta1Pow': np.array([beta1_pow]).astype("float32"),
            'Beta2Pow': np.array([beta2_pow]).astype("float32"),
            'Beta1Tensor': np.array([beta1]).astype("float32"),
            'Beta2Tensor': np.array([beta2]).astype("float32"),
            'EpsilonTensor': np.array([epsilon]).astype("float32"),
            "SkipUpdate": np.array([True]).astype("bool"),
        }

        self.attrs = {'epsilon': epsilon, "coeff": 0.02, "with_decay": True}

        self.outputs = {
            'Moment1Out': moment1,
            'Moment2Out': moment2,
            'ParamOut': param,
            'Beta1PowOut': self.inputs['Beta1Pow'],
            'Beta2PowOut': self.inputs['Beta2Pow'],
        }

    def set_mlu(self):
        self.__class__.use_mlu = True
        self.place = paddle.device.MLUPlace(0)

    def init_dtype(self):
        self.dtype = np.float32

    def test_check_output(self):
        self.check_output_with_place(self.place, atol=1e-5)


class TestAdamOpWithoutDecay(OpTest):
    def setUp(self):
        self.set_mlu()
        self.op_type = "adamw"
        param = np.random.uniform(-1, 1, (102, 105)).astype("float32")
        grad = np.random.uniform(-1, 1, (102, 105)).astype("float32")
        moment1 = np.random.uniform(-1, 1, (102, 105)).astype("float32")
        # The second moment is positive
        moment2 = np.random.random((102, 105)).astype("float32")

        learning_rate = 0.004
        beta1 = 0.78
        beta2 = 0.836
        epsilon = 1e-4
        beta1_pow = beta1**10
        beta2_pow = beta2**10

        self.inputs = {
            'Param': param,
            'Grad': grad,
            'Moment1': moment1,
            'Moment2': moment2,
            'LearningRate': np.array([learning_rate]).astype("float32"),
            'Beta1Pow': np.array([beta1_pow]).astype("float32"),
            'Beta2Pow': np.array([beta2_pow]).astype("float32"),
            'Beta1Tensor': np.array([beta1]).astype("float32"),
            'Beta2Tensor': np.array([beta2]).astype("float32"),
            'EpsilonTensor': np.array([epsilon]).astype("float32"),
            "SkipUpdate": np.array([True]).astype("bool"),
        }

        self.attrs = {'epsilon': epsilon, "coeff": 0.02, "with_decay": False}

        self.outputs = {
            'Moment1Out': moment1,
            'Moment2Out': moment2,
            'ParamOut': param,
            'Beta1PowOut': self.inputs['Beta1Pow'],
            'Beta2PowOut': self.inputs['Beta2Pow'],
        }

    def set_mlu(self):
        self.__class__.use_mlu = True
        self.place = paddle.device.MLUPlace(0)

    def init_dtype(self):
        self.dtype = np.float32

    def test_check_output(self):
        self.check_output_with_place(self.place, atol=1e-5)


class TestNet(unittest.TestCase):
    def _test(self, run_mlu=True):
        main_prog = paddle.static.Program()
        startup_prog = paddle.static.Program()
        main_prog.random_seed = SEED
        startup_prog.random_seed = SEED
        np.random.seed(SEED)

        a_np = np.random.random(size=(32, 32)).astype('float32')
        b_np = np.random.random(size=(32, 32)).astype('float32')
        label_np = np.random.randint(2, size=(32, 1)).astype('int64')

        with paddle.static.program_guard(main_prog, startup_prog):
            a = paddle.static.data(name="a", shape=[32, 32], dtype='float32')
            b = paddle.static.data(name="b", shape=[32, 32], dtype='float32')
            label = paddle.static.data(
                name="label", shape=[32, 1], dtype='int64'
            )

            sum = paddle.add(a, b)
            z = paddle.pow(sum, 2.0)

            fc_1 = fluid.layers.fc(input=z, size=128)
            prediction = fluid.layers.fc(input=fc_1, size=2, act='softmax')

            cost = fluid.layers.cross_entropy(input=prediction, label=label)
            loss = paddle.mean(cost)
            adam = paddle.optimizer.AdamW(learning_rate=0.01, weight_decay=0.02)
            adam.minimize(loss)

        if run_mlu:
            place = paddle.device.MLUPlace(0)
        else:
            place = paddle.CPUPlace()

        exe = paddle.static.Executor(place)
        exe.run(startup_prog)

        print("Start run on {}".format(place))
        for epoch in range(100):

            pred_res, loss_res = exe.run(
                main_prog,
                feed={"a": a_np, "b": b_np, "label": label_np},
                fetch_list=[prediction, loss],
            )
            if epoch % 10 == 0:
                print(
                    "Epoch {} | Prediction[0]: {}, Loss: {}".format(
                        epoch, pred_res[0], loss_res
                    )
                )

        return pred_res, loss_res

    def test_mlu(self):
        mlu_pred, mlu_loss = self._test(True)
        cpu_pred, cpu_loss = self._test(False)
        np.testing.assert_allclose(mlu_pred, cpu_pred, rtol=1e-3)
        np.testing.assert_allclose(mlu_loss, cpu_loss, rtol=1e-3)


if __name__ == '__main__':
    unittest.main()
