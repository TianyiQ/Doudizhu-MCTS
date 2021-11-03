# Doudizhu-MCTS

### en

About this repo:

- This is my course project for *Programming Practice* at PKU.
- The project is completed in teams, but members of my team decided to each pursue a different approach, and in this repo is my approach - which is based on MCTS.

About the algorithm:

- This is an AI for the Chinese card game [Doudizhu](https://www.botzone.org.cn/game/FightTheLandlord2).
- As of Nov. 2021, it [ranks](https://www.botzone.org.cn/game/ranklist/5e36c89c4019f43051e45589) #3 on Botzone among all programs, and #1 among non-RL programs.
  - Its name on the platform is "组合爆炸" (Combinatorial Explosion).
- The algorithm is based on [MCTS with Determinization](https://ieeexplore.ieee.org/document/6031993), with several significant enhancements, including:
  - Assigning priors to the value of MCTS states, based on heuristics.
  - Carrying out bayesian inference on the cards of opponents, based on the cards they already played.
- The hyperparameters are tuned using an evolutionary algorithm, with multiprocessing. (see `/tuning`)
- The source code can be found at `/source/main.cpp`.

Detailed explanation will be added in the future. ~~Well, probably.~~

### zh

关于这个仓库：

- 这是在北京大学《程序设计实习》课程的组队大作业中，我所负责设计和实现的内容（即基于 MCTS 的思路）。
  - 该作业于 2021 上半年完成，但因为拖延症所以直到 2021/11 才上传到 GitHub 。

关于内容：

- 这是一个 [叫分斗地主](https://www.botzone.org.cn/game/FightTheLandlord2) 游戏的 AI 。
- 截止 2021/11 ，其在 Botzone 平台上的 [排名](https://www.botzone.org.cn/game/ranklist/5e36c89c4019f43051e45589) 是所有选手中的 #3 、非 RL 算法中的 #1 。
  - 在平台上的 bot 名称为“组合爆炸”~~，代表着我将在组合数学课中挂科~~。
- 基于 [MCTS with Determinization](https://ieeexplore.ieee.org/document/6031993) 实现，并在原算法基础上进行多处改进（源码见 `/source/main.cpp` ），包括：
  - 用启发式函数给 MCTS 加入先验估值；
  - 根据对手的出牌，来对其手牌进行贝叶斯推断。
- 算法中有大量超参数，采用演化算法进行调参（需要多进程加速，详见 `/tuning` ）。

暂时没有详细的介绍，不过之后可(da)能(gai)会(hui)有(gu)。