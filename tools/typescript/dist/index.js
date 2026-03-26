"use strict";
// AgentOS TypeScript SDK
// Version: 2.0.0
// Last updated: 2026-03-23
var __createBinding = (this && this.__createBinding) || (Object.create ? (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    var desc = Object.getOwnPropertyDescriptor(m, k);
    if (!desc || ("get" in desc ? !m.__esModule : desc.writable || desc.configurable)) {
      desc = { enumerable: true, get: function() { return m[k]; } };
    }
    Object.defineProperty(o, k2, desc);
}) : (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    o[k2] = m[k];
}));
var __exportStar = (this && this.__exportStar) || function(m, exports) {
    for (var p in m) if (p !== "default" && !Object.prototype.hasOwnProperty.call(exports, p)) __createBinding(exports, m, p);
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.VERSION = void 0;
/**
 * AgentOS TypeScript SDK
 *
 * 提供 TypeScript 接口与 AgentOS 系统交互，
 * 包含任务管理、记忆操作、会话管理、技能加载、遥测和系统调用功能。
 */
__exportStar(require("./agent"), exports);
__exportStar(require("./task"), exports);
__exportStar(require("./memory"), exports);
__exportStar(require("./session"), exports);
__exportStar(require("./skill"), exports);
__exportStar(require("./telemetry"), exports);
__exportStar(require("./syscall"), exports);
__exportStar(require("./types"), exports);
__exportStar(require("./errors"), exports);
/** SDK 版本号 */
exports.VERSION = '2.0.0';
