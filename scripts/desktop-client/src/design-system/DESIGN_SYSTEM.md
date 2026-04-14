# AgentOS 设计系统

## 1. 设计理念

### 1.1 核心原则
- **简约至上**：减少视觉噪声，突出核心功能
- **一致性**：统一的设计语言和交互模式
- **可访问性**：确保所有用户都能轻松使用
- **响应式**：适配不同屏幕尺寸
- **性能优先**：流畅的动画和响应速度

### 1.2 设计目标
- 创建直观、易用的用户界面
- 建立统一的视觉语言
- 提供一致的用户体验
- 支持快速开发和维护

## 2. 色彩系统

### 2.1 主色调
| 变量名 | 值 | 描述 |
|--------|-----|------|
| `--primary-color` | `#6366f1` | 主品牌色 |
| `--primary-hover` | `#4f46e5` | 主品牌色悬停状态 |
| `--primary-light` | `rgba(99, 102, 241, 0.1)` | 主品牌色浅色变体 |
| `--primary-dark` | `#3730a3` | 主品牌色深色变体 |

### 2.2 功能色
| 变量名 | 值 | 描述 |
|--------|-----|------|
| `--success-color` | `#22c55e` | 成功状态色 |
| `--warning-color` | `#f59e0b` | 警告状态色 |
| `--error-color` | `#ef4444` | 错误状态色 |
| `--info-color` | `#3b82f6` | 信息状态色 |

### 2.3 中性色
| 变量名 | 值 (Dark) | 值 (Light) | 描述 |
|--------|-----------|------------|------|
| `--bg-primary` | `#0a0a0f` | `#f8fafc` | 主背景色 |
| `--bg-secondary` | `#111118` | `#ffffff` | 次要背景色 |
| `--bg-tertiary` | `#1a1a24` | `#f1f5f9` | 三级背景色 |
| `--text-primary` | `#ededed` | `#0f172a` | 主文本色 |
| `--text-secondary` | `#9ca3af` | `#475569` | 次要文本色 |
| `--text-muted` | `#6b7280` | `#94a3b8` | 静音文本色 |
| `--border-color` | `#27273a` | `#e2e8f0` | 边框色 |
| `--border-subtle` | `#1e1e2e` | `#f1f5f9` | 次要边框色 |

## 3. 字体系统

### 3.1 字体家族
| 变量名 | 值 | 描述 |
|--------|-----|------|
| `--font-family-sans` | `'Inter', -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif` | 无衬线字体 |
| `--font-family-mono` | `'JetBrains Mono', 'Fira Code', 'Cascadia Code', monospace` | 等宽字体 |
| `--font-family-display` | `'Inter', -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif` | 显示字体 |

### 3.2 字体大小
| 变量名 | 值 | 描述 |
|--------|-----|------|
| `--font-size-xs` | `11px` | 超小字体 |
| `--font-size-sm` | `12px` | 小字体 |
| `--font-size-base` | `13px` | 基础字体 |
| `--font-size-md` | `14px` | 中等字体 |
| `--font-size-lg` | `16px` | 大字体 |
| `--font-size-xl` | `18px` | 超大字体 |
| `--font-size-2xl` | `20px` | 2倍大字体 |
| `--font-size-3xl` | `24px` | 3倍大字体 |
| `--font-size-4xl` | `28px` | 4倍大字体 |

### 3.3 行高
| 变量名 | 值 | 描述 |
|--------|-----|------|
| `--line-height-tight` | `1.2` | 紧凑行高 |
| `--line-height-normal` | `1.4` | 正常行高 |
| `--line-height-relaxed` | `1.6` | 宽松行高 |

### 3.4 字间距
| 变量名 | 值 | 描述 |
|--------|-----|------|
| `--letter-spacing-tight` | `-0.02em` | 紧凑字间距 |
| `--letter-spacing-normal` | `0` | 正常字间距 |
| `--letter-spacing-wide` | `0.02em` | 宽字间距 |
| `--letter-spacing-wider` | `0.05em` | 更宽字间距 |

## 4. 间距系统

| 变量名 | 值 | 描述 |
|--------|-----|------|
| `--space-1` | `4px` | 1单位间距 |
| `--space-2` | `8px` | 2单位间距 |
| `--space-3` | `12px` | 3单位间距 |
| `--space-4` | `16px` | 4单位间距 |
| `--space-5` | `20px` | 5单位间距 |
| `--space-6` | `24px` | 6单位间距 |
| `--space-8` | `32px` | 8单位间距 |
| `--space-10` | `40px` | 10单位间距 |

## 5. 边框半径

| 变量名 | 值 | 描述 |
|--------|-----|------|
| `--radius-sm` | `4px` | 小圆角 |
| `--radius-md` | `8px` | 中等圆角 |
| `--radius-lg` | `12px` | 大圆角 |
| `--radius-xl` | `16px` | 超大圆角 |
| `--radius-full` | `9999px` | 完全圆角 |

## 6. 阴影系统

| 变量名 | 值 | 描述 |
|--------|-----|------|
| `--shadow-sm` | `0 1px 2px rgba(0, 0, 0, 0.05)` | 小阴影 |
| `--shadow-md` | `0 2px 4px rgba(0, 0, 0, 0.05)` | 中等阴影 |
| `--shadow-lg` | `0 4px 8px rgba(0, 0, 0, 0.05)` | 大阴影 |
| `--shadow-xl` | `0 8px 16px rgba(0, 0, 0, 0.05)` | 超大阴影 |
| `--shadow-card` | `0 2px 4px rgba(0, 0, 0, 0.04), 0 1px 2px rgba(0, 0, 0, 0.02)` | 卡片阴影 |
| `--shadow-card-hover` | `0 4px 12px rgba(0, 0, 0, 0.06), 0 2px 4px rgba(0, 0, 0, 0.04)` | 卡片悬停阴影 |
| `--shadow-glow` | `0 0 24px rgba(99, 102, 241, 0.3)` | 发光阴影 |

## 7. 过渡动画

| 变量名 | 值 | 描述 |
|--------|-----|------|
| `--transition-fast` | `150ms ease` | 快速过渡 |
| `--transition-base` | `200ms ease` | 基础过渡 |
| `--transition-slow` | `300ms ease` | 慢速过渡 |
| `--transition-spring` | `300ms cubic-bezier(0.34, 1.56, 0.64, 1)` | 弹簧过渡 |

## 8. 组件库

### 8.1 Button 组件

**Props**:
- `children`: React.ReactNode - 按钮内容
- `onClick`: (e: React.MouseEvent<HTMLButtonElement>) => void - 点击事件回调
- `variant`: 'primary' | 'secondary' | 'ghost' | 'danger' | 'success' - 按钮变体
- `size`: 'sm' | 'md' | 'lg' - 按钮大小
- `disabled`: boolean - 是否禁用
- `loading`: boolean - 是否加载中
- `className`: string - 自定义类名
- `style`: React.CSSProperties - 自定义样式
- `title`: string - 按钮标题

**示例**:
```tsx
<Button variant="primary" size="md" onClick={() => console.log('Clicked')}>
  <Send size={16} />
  发送消息
</Button>
```

### 8.2 Input 组件

**Props**:
- `value`: string - 输入值
- `onChange`: (e: React.ChangeEvent<HTMLInputElement>) => void - 输入变化回调
- `placeholder`: string - 占位文本
- `type`: string - 输入类型
- `disabled`: boolean - 是否禁用
- `className`: string - 自定义类名
- `style`: React.CSSProperties - 自定义样式
- `onKeyDown`: (e: React.KeyboardEvent<HTMLInputElement>) => void - 键盘事件回调
- `autoFocus`: boolean - 是否自动聚焦

**示例**:
```tsx
<Input
  value={inputValue}
  onChange={(e) => setInputValue(e.target.value)}
  placeholder="请输入内容"
  type="text"
/>
```

### 8.3 Card 组件

**Props**:
- `children`: React.ReactNode - 卡片内容
- `className`: string - 自定义类名
- `style`: React.CSSProperties - 自定义样式
- `padding`: number - 内边距
- `onClick`: (e: React.MouseEvent<HTMLDivElement>) => void - 点击事件回调
- `onMouseEnter`: (e: React.MouseEvent<HTMLDivElement>) => void - 鼠标进入事件回调
- `onMouseLeave`: (e: React.MouseEvent<HTMLDivElement>) => void - 鼠标离开事件回调

**示例**:
```tsx
<Card
  style={{ boxShadow: 'var(--shadow-card)' }}
  onMouseEnter={(e) => e.currentTarget.style.transform = 'translateY(-2px)'}
  onMouseLeave={(e) => e.currentTarget.style.transform = 'translateY(0)'}
>
  <h3>卡片标题</h3>
  <p>卡片内容</p>
</Card>
```

### 8.4 PageLayout 组件

**Props**:
- `title`: string - 页面标题
- `subtitle`: string - 页面副标题
- `children`: React.ReactNode - 页面内容
- `actions`: React.ReactNode - 页面操作按钮

**示例**:
```tsx
<PageLayout
  title="Dashboard"
  subtitle="系统概览与监控"
  actions={
    <Button variant="primary" onClick={() => console.log('Refresh')}>
      <RefreshCw size={16} />
      刷新
    </Button>
  }
>
  {/* 页面内容 */}
</PageLayout>
```

### 8.5 OperationFeedback 组件

**Props**:
- `type`: 'success' | 'error' | 'warning' | 'info' - 反馈类型
- `title`: string - 反馈标题
- `message`: string - 反馈消息
- `duration`: number - 显示时长
- `onClose`: () => void - 关闭回调

**示例**:
```tsx
import { useOperationFeedback } from '../components/OperationFeedback';

const { success, error, warning, info } = useOperationFeedback();

// 使用
success('操作成功', '数据已保存');
error('操作失败', '请检查网络连接');
```

### 8.6 StepByStepGuide 组件

**Props**:
- `onComplete`: () => void - 引导完成回调

**示例**:
```tsx
<StepByStepGuide onComplete={() => console.log('Guide completed')} />
```

## 9. 布局系统

### 9.1 应用布局
- 左侧导航栏：宽度 `--sidebar-width`
- 顶部标题栏：高度 `--header-height`
- 底部状态栏：高度 `--statusbar-height`
- 主内容区域：自适应剩余空间

### 9.2 响应式设计
- 桌面：> 1280px
- 平板：768px - 1280px
- 移动：< 768px

## 10. 图标系统

### 10.1 图标库
- 使用 Lucide React 图标库
- 图标大小：16px、18px、20px、24px
- 图标颜色：使用主题色彩变量

### 10.2 图标使用规范
- 保持图标风格一致
- 同一功能使用相同图标
- 图标尺寸与周围元素协调

## 11. 交互设计

### 11.1 反馈机制
- 按钮点击：状态变化 + 动画
- 表单提交：加载状态 + 结果反馈
- 错误处理：清晰的错误提示
- 成功操作：确认反馈

### 11.2 动画原则
- 保持动画简洁
- 动画时长合理
- 避免过度动画
- 动画应该有意义

## 12. 可访问性

### 12.1 键盘导航
- 所有交互元素支持键盘访问
- 合理的Tab顺序
- 清晰的焦点状态

### 12.2 屏幕阅读器
- 适当的ARIA属性
- 语义化HTML
- 文本替代

### 12.3 颜色对比度
- 文本与背景对比度符合WCAG标准
- 确保所有用户都能看清内容

## 13. 开发规范

### 13.1 代码风格
- 使用TypeScript
- 遵循ESLint规范
- 组件命名采用PascalCase
- 变量命名采用camelCase
- 常量命名采用UPPER_SNAKE_CASE

### 13.2 组件开发
- 组件应该是可复用的
- 组件应该有清晰的API
- 组件应该有适当的文档
- 组件应该考虑性能优化

### 13.3 样式管理
- 使用CSS变量
- 避免硬编码值
- 优先使用工具类
- 保持样式一致性

## 14. 测试策略

### 14.1 单元测试
- 测试组件渲染
- 测试组件交互
- 测试组件边界情况

### 14.2 集成测试
- 测试组件组合
- 测试页面流程
- 测试用户场景

### 14.3 端到端测试
- 测试完整用户流程
- 测试跨浏览器兼容性
- 测试响应式设计

## 15. 版本控制

### 15.1 设计系统版本
- 主版本：重大变更
- 次版本：新增功能
- 补丁版本：bug修复

### 15.2 变更记录
- 记录设计系统的所有变更
- 提供迁移指南
- 保持向后兼容性

## 16. 维护与更新

### 16.1 设计系统维护
- 定期更新设计系统
- 收集用户反馈
- 持续优化组件

### 16.2 最佳实践
- 遵循设计系统指南
- 定期审查设计系统使用情况
- 鼓励团队成员贡献改进

## 17. 资源与工具

### 17.1 设计工具
- Figma：设计协作
- Sketch：界面设计
- Adobe XD：原型设计

### 17.2 开发工具
- VS Code：代码编辑器
- ESLint：代码质量
- Prettier：代码格式化
- TypeScript：类型检查

### 17.3 文档工具
- Markdown：文档编写
- Storybook：组件文档
- Docusaurus：技术文档

## 18. 结语

AgentOS 设计系统是一个不断演进的项目，旨在为用户提供一致、美观、易用的界面体验。通过遵循本设计系统的规范，我们可以确保整个应用的视觉风格统一，提高开发效率，同时为用户创造更好的使用体验。

设计系统不是一成不变的，它应该随着产品的发展和用户需求的变化而不断进化。我们鼓励团队成员积极参与设计系统的改进和完善，共同打造更好的 AgentOS 体验。
