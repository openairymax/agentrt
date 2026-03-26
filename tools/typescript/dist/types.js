"use strict";
// AgentOS TypeScript SDK Types
// Version: 2.0.0
// Last updated: 2026-03-23
Object.defineProperty(exports, "__esModule", { value: true });
exports.MemoryLayer = exports.TaskStatus = void 0;
/** 任务状态枚举 */
var TaskStatus;
(function (TaskStatus) {
    TaskStatus["PENDING"] = "pending";
    TaskStatus["RUNNING"] = "running";
    TaskStatus["COMPLETED"] = "completed";
    TaskStatus["FAILED"] = "failed";
    TaskStatus["CANCELLED"] = "cancelled";
})(TaskStatus || (exports.TaskStatus = TaskStatus = {}));
/** 记忆层级枚举 */
var MemoryLayer;
(function (MemoryLayer) {
    MemoryLayer["RAW"] = "RAW";
    MemoryLayer["WORKING"] = "WORKING";
    MemoryLayer["LONG_TERM"] = "LONG_TERM";
    MemoryLayer["EPISODIC"] = "EPISODIC";
})(MemoryLayer || (exports.MemoryLayer = MemoryLayer = {}));
