#import <Cocoa/Cocoa.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

typedef void (^ToolCompletion)(int, NSString *, NSString *);

@interface AppDelegate : NSObject <NSApplicationDelegate>
@property NSWindow *window;
@property NSTextField *deviceLabel;
@property NSTextField *firmwareLabel;
@property NSTextField *statusLabel;
@property NSProgressIndicator *progress;
@property NSTextView *logView;
@property NSButton *checkButton;
@property NSButton *chooseButton;
@property NSButton *upgradeButton;
@property NSURL *firmwareURL;
@end

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    [self buildWindow];
    self.firmwareURL = [[NSBundle mainBundle]
        URLForResource:@"G6HE_v1.0.0+84_202609101503.signed"
        withExtension:@"bin"
        subdirectory:@"Firmware"];
    [self refreshFirmwareLabel];
    [self.window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
    [self probeDevice:nil];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
    return YES;
}

- (NSTextField *)label:(NSString *)text size:(CGFloat)size weight:(NSFontWeight)weight {
    NSTextField *label = [NSTextField labelWithString:text];
    label.font = [NSFont systemFontOfSize:size weight:weight];
    return label;
}

- (void)buildWindow {
    self.window = [[NSWindow alloc]
        initWithContentRect:NSMakeRect(0, 0, 680, 520)
        styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable
        backing:NSBackingStoreBuffered
        defer:NO];
    self.window.title = @"G6 HE Firmware Updater";
    self.window.releasedWhenClosed = NO;
    [self.window center];

    NSTextField *title = [self label:@"G6 HE macOS 固件升级" size:24 weight:NSFontWeightSemibold];
    NSTextField *subtitle = [NSTextField wrappingLabelWithString:
        @"升级前会校验机型、固件容器和传输参数。升级期间请勿拔线或让电脑休眠。"];
    subtitle.textColor = NSColor.secondaryLabelColor;

    self.deviceLabel = [self label:@"设备：等待检测" size:14 weight:NSFontWeightMedium];
    self.firmwareLabel = [self label:@"固件：内置 1.0.0+84" size:13 weight:NSFontWeightRegular];
    self.statusLabel = [NSTextField wrappingLabelWithString:
        @"请用 USB 线直连 G6 HE，并切换到有线模式。"];
    self.statusLabel.textColor = NSColor.secondaryLabelColor;

    self.progress = [[NSProgressIndicator alloc] init];
    self.progress.indeterminate = NO;
    self.progress.minValue = 0;
    self.progress.maxValue = 100;

    self.checkButton = [NSButton buttonWithTitle:@"检测设备" target:self action:@selector(probeDevice:)];
    self.chooseButton = [NSButton buttonWithTitle:@"选择固件…" target:self action:@selector(chooseFirmware:)];
    self.upgradeButton = [NSButton buttonWithTitle:@"开始升级" target:self action:@selector(beginUpgrade:)];
    self.upgradeButton.keyEquivalent = @"\r";

    NSStackView *buttons = [NSStackView stackViewWithViews:
        @[self.checkButton, self.chooseButton, self.upgradeButton]];
    buttons.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    buttons.spacing = 10;

    self.logView = [[NSTextView alloc] init];
    self.logView.editable = NO;
    self.logView.selectable = YES;
    self.logView.font = [NSFont monospacedSystemFontOfSize:11 weight:NSFontWeightRegular];
    self.logView.textContainerInset = NSMakeSize(8, 8);
    NSScrollView *scroll = [[NSScrollView alloc] init];
    scroll.borderType = NSBezelBorder;
    scroll.hasVerticalScroller = YES;
    scroll.documentView = self.logView;
    scroll.translatesAutoresizingMaskIntoConstraints = NO;
    [scroll.heightAnchor constraintEqualToConstant:245].active = YES;

    NSStackView *stack = [NSStackView stackViewWithViews:
        @[title, subtitle, self.deviceLabel, self.firmwareLabel, self.statusLabel,
          self.progress, buttons, scroll]];
    stack.orientation = NSUserInterfaceLayoutOrientationVertical;
    stack.alignment = NSLayoutAttributeLeading;
    stack.spacing = 12;
    stack.translatesAutoresizingMaskIntoConstraints = NO;
    self.window.contentView = [[NSView alloc] init];
    [self.window.contentView addSubview:stack];

    [NSLayoutConstraint activateConstraints:@[
        [stack.leadingAnchor constraintEqualToAnchor:self.window.contentView.leadingAnchor constant:28],
        [stack.trailingAnchor constraintEqualToAnchor:self.window.contentView.trailingAnchor constant:-28],
        [stack.topAnchor constraintEqualToAnchor:self.window.contentView.topAnchor constant:26],
        [stack.bottomAnchor constraintLessThanOrEqualToAnchor:self.window.contentView.bottomAnchor constant:-24],
        [subtitle.widthAnchor constraintEqualToAnchor:stack.widthAnchor],
        [self.statusLabel.widthAnchor constraintEqualToAnchor:stack.widthAnchor],
        [self.progress.widthAnchor constraintEqualToAnchor:stack.widthAnchor],
        [scroll.widthAnchor constraintEqualToAnchor:stack.widthAnchor],
    ]];
}

- (void)refreshFirmwareLabel {
    self.firmwareLabel.stringValue = self.firmwareURL
        ? [NSString stringWithFormat:@"固件：%@", self.firmwareURL.lastPathComponent]
        : @"固件：未找到";
}

- (void)setBusy:(BOOL)busy message:(NSString *)message {
    self.checkButton.enabled = !busy;
    self.chooseButton.enabled = !busy;
    self.upgradeButton.enabled = !busy;
    self.statusLabel.stringValue = message;
}

- (void)appendLog:(NSString *)text {
    if (!text.length) return;
    NSString *cleaned = [text stringByReplacingOccurrencesOfString:@"\r" withString:@""];
    [self.logView.textStorage appendAttributedString:
        [[NSAttributedString alloc] initWithString:cleaned]];
    [self.logView scrollToEndOfDocument:nil];
    for (NSString *line in [cleaned componentsSeparatedByString:@"\n"]) {
        if (![line hasPrefix:@"Progress: "]) continue;
        NSString *rest = [line substringFromIndex:@"Progress: ".length];
        double percent = [[[rest componentsSeparatedByString:@"%"] firstObject] doubleValue];
        self.progress.doubleValue = percent;
        self.statusLabel.stringValue = [NSString stringWithFormat:@"正在写入固件：%.0f%%", percent];
    }
}

- (NSDictionary *)parseResult:(NSString *)output {
    NSData *data = [output dataUsingEncoding:NSUTF8StringEncoding];
    NSDictionary *root = data ? [NSJSONSerialization JSONObjectWithData:data options:0 error:nil] : nil;
    if (![root isKindOfClass:NSDictionary.class] || ![root[@"ok"] boolValue]) return nil;
    return [root[@"result"] isKindOfClass:NSDictionary.class] ? root[@"result"] : nil;
}

- (void)showError:(NSString *)title details:(NSString *)details {
    NSAlert *alert = [[NSAlert alloc] init];
    alert.alertStyle = NSAlertStyleCritical;
    alert.messageText = title;
    alert.informativeText = details.length ? details : @"请查看窗口中的日志。";
    [alert runModal];
}

- (void)runTool:(NSArray<NSString *> *)arguments
           busy:(NSString *)busyMessage
     completion:(ToolCompletion)completion {
    NSString *path = [[NSBundle mainBundle] pathForAuxiliaryExecutable:@"g6he-updater-cli"];
    if (!path) {
        [self showError:@"升级引擎缺失" details:@"请重新安装应用。"];
        return;
    }
    [self setBusy:YES message:busyMessage];
    [self appendLog:[NSString stringWithFormat:@"\n$ %@\n", [arguments componentsJoinedByString:@" "]]];

    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        NSTask *task = [[NSTask alloc] init];
        NSPipe *outPipe = [NSPipe pipe];
        NSPipe *errPipe = [NSPipe pipe];
        task.executableURL = [NSURL fileURLWithPath:path];
        task.arguments = arguments;
        task.standardOutput = outPipe;
        task.standardError = errPipe;
        NSError *launchError = nil;
        if (![task launchAndReturnError:&launchError]) {
            dispatch_async(dispatch_get_main_queue(), ^{
                [self setBusy:NO message:@"无法启动升级引擎"];
                [self showError:@"无法启动升级引擎" details:launchError.localizedDescription];
                completion(-1, @"", launchError.localizedDescription);
            });
            return;
        }
        NSData *outData = [outPipe.fileHandleForReading readDataToEndOfFile];
        NSData *errData = [errPipe.fileHandleForReading readDataToEndOfFile];
        [task waitUntilExit];
        NSString *output = [[NSString alloc] initWithData:outData encoding:NSUTF8StringEncoding] ?: @"";
        NSString *error = [[NSString alloc] initWithData:errData encoding:NSUTF8StringEncoding] ?: @"";
        dispatch_async(dispatch_get_main_queue(), ^{
            [self appendLog:error];
            [self appendLog:output];
            [self setBusy:NO message:task.terminationStatus == 0 ? @"操作完成" : @"操作未完成"];
            completion(task.terminationStatus, output, error);
        });
    });
}

- (void)probeDevice:(id)sender {
    self.progress.doubleValue = 0;
    [self runTool:@[@"probe"] busy:@"正在检测设备…" completion:
        ^(int status, NSString *output, NSString *error) {
            NSDictionary *result = status == 0 ? [self parseResult:output] : nil;
            if (!result) {
                self.deviceLabel.stringValue = @"设备：未检测到有线 G6 HE";
                [self showError:@"未检测到 G6 HE" details:error];
                return;
            }
            self.deviceLabel.stringValue = [NSString stringWithFormat:@"设备：%@ · 当前固件 %@",
                result[@"model"] ?: @"未知", result[@"firmware_version"] ?: @"未知"];
            self.statusLabel.stringValue = @"设备检测通过，可以进行固件校验或升级。";
        }];
}

- (void)chooseFirmware:(id)sender {
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    panel.title = @"选择 G6 HE 签名固件";
    panel.allowedContentTypes = @[[UTType typeWithFilenameExtension:@"bin"]];
    panel.allowsMultipleSelection = NO;
    if ([panel runModal] == NSModalResponseOK) {
        self.firmwareURL = panel.URL;
        [self refreshFirmwareLabel];
    }
}

- (void)beginUpgrade:(id)sender {
    if (!self.firmwareURL) {
        [self showError:@"未找到固件" details:@"请选择 .signed.bin 固件文件。"];
        return;
    }
    self.progress.doubleValue = 0;
    NSArray *checkArgs = @[@"upgrade", self.firmwareURL.path, @"--dry-run"];
    [self runTool:checkArgs busy:@"正在校验设备和固件…" completion:
        ^(int status, NSString *output, NSString *error) {
            NSDictionary *result = status == 0 ? [self parseResult:output] : nil;
            if (!result) {
                [self showError:@"升级前检查未通过" details:error];
                return;
            }
            NSString *target = result[@"target_version"] ?: @"未知";
            if ([result[@"status"] isEqual:@"already_current"]) {
                self.progress.doubleValue = 100;
                self.statusLabel.stringValue = [NSString stringWithFormat:@"设备已经是目标版本 %@。", target];
                return;
            }

            NSAlert *alert = [[NSAlert alloc] init];
            alert.alertStyle = NSAlertStyleWarning;
            alert.messageText = [NSString stringWithFormat:@"确认升级到 %@？", target];
            alert.informativeText = @"升级期间请保持 USB 连接，不要关闭应用或让电脑休眠。";
            [alert addButtonWithTitle:@"开始升级"];
            [alert addButtonWithTitle:@"取消"];
            if ([alert runModal] != NSAlertFirstButtonReturn) {
                self.statusLabel.stringValue = @"已取消，没有写入固件。";
                return;
            }

            NSArray *args = @[@"upgrade", self.firmwareURL.path, @"--confirm", @"54LMG6HE"];
            [self runTool:args busy:@"正在升级，请勿拔线…" completion:
                ^(int finalStatus, NSString *finalOutput, NSString *finalError) {
                    NSDictionary *finalResult = finalStatus == 0 ? [self parseResult:finalOutput] : nil;
                    if (!finalResult) {
                        [self showError:@"升级未完成" details:finalError];
                        return;
                    }
                    if ([finalResult[@"verified_after_restart"] boolValue]) {
                        self.progress.doubleValue = 100;
                        self.statusLabel.stringValue = [NSString stringWithFormat:
                            @"升级成功，重启后已验证版本 %@。", finalResult[@"target_version"] ?: target];
                    } else {
                        [self showError:@"版本验证未通过" details:@"请查看日志并重新连接设备。"];
                    }
                }];
        }];
}

@end

int main(int argc, const char *argv[]) {
    @autoreleasepool {
        NSApplication *app = NSApplication.sharedApplication;
        AppDelegate *delegate = [[AppDelegate alloc] init];
        app.delegate = delegate;
        [app setActivationPolicy:NSApplicationActivationPolicyRegular];
        [app run];
    }
    return 0;
}
