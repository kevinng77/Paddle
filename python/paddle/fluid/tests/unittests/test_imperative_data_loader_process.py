# Copyright (c) 2019 PaddlePaddle Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import multiprocessing
import queue
import unittest

import numpy as np

import paddle.fluid as fluid
from paddle.fluid.framework import _test_eager_guard
from paddle.fluid.reader import _reader_process_loop


def get_random_images_and_labels(image_shape, label_shape):
    image = np.random.random(size=image_shape).astype('float32')
    label = np.random.random(size=label_shape).astype('int64')
    return image, label


def batch_generator_creator(batch_size, batch_num):
    def __reader__():
        for _ in range(batch_num):
            batch_image, batch_label = get_random_images_and_labels(
                [batch_size, 784], [batch_size, 1]
            )
            yield batch_image, batch_label

    return __reader__


# NOTE: coverage CI can't cover child process code, so need these test.
# Here test child process loop function in main process
class TestDygraphDataLoaderProcess(unittest.TestCase):
    def setUp(self):
        self.batch_size = 8
        self.batch_num = 4
        self.epoch_num = 2
        self.capacity = 2

    def func_test_reader_process_loop(self):
        # This unittest's memory mapped files needs to be cleaned manually
        def __clear_process__(util_queue):
            while True:
                try:
                    util_queue.get_nowait()
                except queue.Empty:
                    break

        with fluid.dygraph.guard():
            loader = fluid.io.DataLoader.from_generator(
                capacity=self.batch_num + 1, use_multiprocess=True
            )
            loader.set_batch_generator(
                batch_generator_creator(self.batch_size, self.batch_num),
                places=fluid.CPUPlace(),
            )
            loader._data_queue = queue.Queue(self.batch_num + 1)
            _reader_process_loop(loader._batch_reader, loader._data_queue)
            # For clean memory mapped files
            util_queue = multiprocessing.Queue(self.batch_num + 1)
            for _ in range(self.batch_num):
                data = loader._data_queue.get(timeout=10)
                util_queue.put(data)

            # Clean up memory mapped files
            clear_process = multiprocessing.Process(
                target=__clear_process__, args=(util_queue,)
            )
            clear_process.start()

    def test_reader_process_loop(self):
        with _test_eager_guard():
            self.func_test_reader_process_loop()
        self.func_test_reader_process_loop()

    def func_test_reader_process_loop_simple_none(self):
        def none_sample_genarator(batch_num):
            def __reader__():
                for _ in range(batch_num):
                    yield None

            return __reader__

        with fluid.dygraph.guard():
            loader = fluid.io.DataLoader.from_generator(
                capacity=self.batch_num + 1, use_multiprocess=True
            )
            loader.set_batch_generator(
                none_sample_genarator(self.batch_num), places=fluid.CPUPlace()
            )
            loader._data_queue = queue.Queue(self.batch_num + 1)
            exception = None
            try:
                _reader_process_loop(loader._batch_reader, loader._data_queue)
            except ValueError as ex:
                exception = ex
            self.assertIsNotNone(exception)

    def test_reader_process_loop_simple_none(self):
        with _test_eager_guard():
            self.func_test_reader_process_loop_simple_none()
        self.func_test_reader_process_loop_simple_none()


if __name__ == '__main__':
    unittest.main()
