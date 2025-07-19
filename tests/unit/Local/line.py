from collections import defaultdict, deque
from typing import *

# Definition for a binary tree node.


class Solution:
    def maxSlidingWindow(self, nums: List[int], k: int) -> List[int]:
        ans = [0] * (len(nums) - k + 1)  # 窗口个数
        q = deque()  # 双端队列

        for i, x in enumerate(nums):
            # 1. 右边入
            while q and nums[q[-1]] <= x:
                q.pop()  # 维护 q 的单调性
            q.append(i)

            # 2. 左边出
            left = i - k + 1  # 窗口左端点
            if q[0] < left:  # 队首已经离开窗口了
                q.popleft()

            # 3. 在窗口左端点处记录答案
            if left >= 0:
                # 由于队首到队尾单调递减，所以窗口最大值就在队首
                ans[left] = nums[q[0]]

        return ans
