/* UIKit shell for the complete translated engine. Not the old disc probe. */
#import <UIKit/UIKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import <AVFoundation/AVFoundation.h>
#import <QuartzCore/QuartzCore.h>
#include <stdatomic.h>
#include <time.h>
#include <stdio.h>
#include "native_platform.h"
#include "native_entry.h"
#include "pc/platform/platform.h"

@interface LekakNativeController : UIViewController <UIDocumentPickerDelegate> {
    atomic_int paused;
    NSLock *frameLock;
    NSData *latestFrame;
    int latestWidth,latestHeight;
    BOOL frameChanged,started,importing,finished;
    uint16_t heldButtons;
    NSTimeInterval launchTime;
    UIImageView *screen;
    UILabel *status;
    UIButton *choose,*launch,*share;
    NSMutableArray<UIButton *> *controls;
    CADisplayLink *displayLink;
    AVAudioEngine *audioEngine;
    AVAudioSourceNode *audioSource;
    NSString *discPath,*userRoot,*lastError;
    NSMutableDictionary *report;
}
- (void)acceptFrame:(const uint32_t *)pixels width:(int)width height:(int)height;
- (void)showFailure:(NSString *)title message:(NSString *)message;
- (int)startAudio:(void (*)(int16_t *,size_t))mix;
- (void)pump;
@end

static int openGame(void *context) {
    (void)context;
    return 0;
}
static void presentGame(void *context,const uint32_t *pixels,int width,int height) {
    [(__bridge LekakNativeController *)context acceptFrame:pixels width:width height:height];
}
static void errorGame(void *context,const char *title,const char *message) {
    NSString *t=title?[NSString stringWithUTF8String:title]:@"Lekak";
    NSString *m=message?[NSString stringWithUTF8String:message]:@"Unknown error";
    LekakNativeController *host=(__bridge LekakNativeController *)context;
    dispatch_async(dispatch_get_main_queue(),^{[host showFailure:t?:@"Lekak" message:m?:@"Unknown error"];});
}
static int audioGame(void *context,void (*mix)(int16_t *,size_t)) {
    __block int result=-1;
    LekakNativeController *host=(__bridge LekakNativeController *)context;
    dispatch_sync(dispatch_get_main_queue(),^{result=[host startAudio:mix];});
    return result;
}
static void pumpGame(void *context){[(__bridge LekakNativeController *)context pump];}

@implementation LekakNativeController
- (UIButton *)button:(NSString *)name action:(SEL)action {
    UIButton *b=[UIButton buttonWithType:UIButtonTypeSystem];
    [b setTitle:name forState:UIControlStateNormal];
    [b setTitleColor:UIColor.whiteColor forState:UIControlStateNormal];
    b.backgroundColor=[UIColor colorWithWhite:0.18 alpha:0.95];
    b.layer.cornerRadius=10;b.titleLabel.font=[UIFont boldSystemFontOfSize:15];
    if(action)[b addTarget:self action:action forControlEvents:UIControlEventTouchUpInside];
    [self.view addSubview:b];return b;
}
- (void)viewDidLoad {
    [super viewDidLoad];self.view.backgroundColor=UIColor.blackColor;
    atomic_init(&paused,0);frameLock=[NSLock new];controls=[NSMutableArray new];
    NSString *docs=NSSearchPathForDirectoriesInDomains(NSDocumentDirectory,NSUserDomainMask,YES).firstObject;
    NSString *support=NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory,NSUserDomainMask,YES).firstObject;
    userRoot=[support stringByAppendingPathComponent:@"LekakNative"];
    NSFileManager *fm=NSFileManager.defaultManager;
    [fm createDirectoryAtPath:userRoot withIntermediateDirectories:YES attributes:nil error:nil];
    NSString *discs=[docs stringByAppendingPathComponent:@"LekakNative"];
    [fm createDirectoryAtPath:discs withIntermediateDirectories:YES attributes:nil error:nil];
    discPath=[discs stringByAppendingPathComponent:@"game.bin"];
    choose=[self button:@"BIN" action:@selector(chooseDisc)];
    launch=[self button:@"Lancer / Play" action:@selector(startGame)];
    share=[self button:@"Rapport / Report" action:@selector(shareReport)];
    screen=[UIImageView new];screen.backgroundColor=UIColor.blackColor;
    screen.contentMode=UIViewContentModeScaleAspectFit;
    screen.layer.magnificationFilter=kCAFilterNearest;screen.layer.minificationFilter=kCAFilterNearest;
    [self.view addSubview:screen];
    status=[UILabel new];status.textColor=UIColor.whiteColor;status.numberOfLines=0;
    status.font=[UIFont systemFontOfSize:13];status.textAlignment=NSTextAlignmentCenter;
    status.text=@"Lekak — moteur natif iPhone\nChoisis ton BIN USA, puis Lancer. / Choose your USA BIN, then Play.";
    [self.view addSubview:status];
    NSArray *names=@[@"↑",@"←",@"↓",@"→",@"△",@"□",@"×",@"○",@"L1",@"L2",@"R1",@"R2",@"Select",@"Start"];
    const uint16_t bits[]={0x10,0x80,0x40,0x20,0x1000,0x8000,0x4000,0x2000,0x400,0x100,0x800,0x200,1,8};
    for(NSUInteger i=0;i<names.count;i++) {
        UIButton *b=[self button:names[i] action:NULL];b.tag=bits[i];b.enabled=NO;
        b.backgroundColor=[UIColor colorWithWhite:0.12 alpha:0.65];
        [b addTarget:self action:@selector(press:) forControlEvents:UIControlEventTouchDown|UIControlEventTouchDragEnter];
        [b addTarget:self action:@selector(release:) forControlEvents:UIControlEventTouchUpInside|UIControlEventTouchUpOutside|UIControlEventTouchCancel|UIControlEventTouchDragExit];
        [controls addObject:b];
    }
    displayLink=[CADisplayLink displayLinkWithTarget:self selector:@selector(displayFrame)];
    displayLink.preferredFramesPerSecond=60;
    [displayLink addToRunLoop:NSRunLoop.mainRunLoop forMode:NSRunLoopCommonModes];
    NSNotificationCenter *nc=NSNotificationCenter.defaultCenter;
    [nc addObserver:self selector:@selector(suspendGame) name:UIApplicationWillResignActiveNotification object:nil];
    [nc addObserver:self selector:@selector(resumeGame) name:UIApplicationDidBecomeActiveNotification object:nil];
    [nc addObserver:self selector:@selector(audioInterrupted:) name:AVAudioSessionInterruptionNotification object:nil];
    report=[NSMutableDictionary dictionaryWithDictionary:@{@"build":@23,@"engine":@"full translated engine",@"device_tested":@NO}];
}
- (void)viewDidLayoutSubviews {
    [super viewDidLayoutSubviews];CGRect box=UIEdgeInsetsInsetRect(self.view.bounds,self.view.safeAreaInsets);
    CGFloat x=box.origin.x,y=box.origin.y,w=box.size.width,h=box.size.height;
    choose.frame=CGRectMake(x+8,y+4,52,34);launch.frame=CGRectMake(x+68,y+4,132,34);
    share.frame=CGRectMake(x+w-142,y+4,134,34);
    BOOL landscape=w>h;CGFloat size=landscape?44:48,gap=6,step=size+gap;
    CGFloat left=x+10,right=x+w-10-3*step,base=y+h-3*step-8;
    const int col[]={1,0,1,2,1,0,1,2};const int row[]={0,1,2,1,0,1,2,1};
    for(int i=0;i<8;i++)controls[i].frame=CGRectMake((i<4?left:right)+col[i]*step,base+row[i]*step,size,size);
    for(int i=8;i<12;i++) {
        CGFloat bx=i<10?left:right+step;
        controls[i].frame=CGRectMake(bx+(i%2)*step,base-step,size,size-4);
    }
    controls[12].frame=CGRectMake(x+w/2-76,y+h-size-8,70,size-4);
    controls[13].frame=CGRectMake(x+w/2+6,y+h-size-8,70,size-4);
    screen.frame=landscape?CGRectMake(x+4,y+40,w-8,MAX(1,h-44)):
        CGRectMake(x+2,y+40,w-4,MAX(1,h-4*step-70));
    /* Controls overlay the margins; preserve the game aspect ratio. */
    for(UIButton *control in controls)[self.view bringSubviewToFront:control];
    status.frame=screen.frame;
}
- (void)press:(UIButton *)b{heldButtons|=(uint16_t)b.tag;LekakNative_SetPad(0,heldButtons,1);}
- (void)release:(UIButton *)b{heldButtons&=~(uint16_t)b.tag;LekakNative_SetPad(0,heldButtons,1);}
- (void)suspendGame {
    atomic_store(&paused,1);heldButtons=0;LekakNative_ReleasePads();
    [audioEngine pause];UIApplication.sharedApplication.idleTimerDisabled=NO;
}
- (void)resumeGame {
    if(started&&!finished) {
        NSError *error=nil;[[AVAudioSession sharedInstance] setActive:YES error:&error];
        if(audioEngine&&!audioEngine.isRunning)[audioEngine startAndReturnError:&error];
        if(error)status.text=error.localizedDescription;
        UIApplication.sharedApplication.idleTimerDisabled=YES;
    }
    atomic_store(&paused,0);
}
- (void)audioInterrupted:(NSNotification *)n {
    NSNumber *type=n.userInfo[AVAudioSessionInterruptionTypeKey];
    if(type.unsignedIntegerValue==AVAudioSessionInterruptionTypeBegan)[self suspendGame];
    else if(UIApplication.sharedApplication.applicationState==UIApplicationStateActive)[self resumeGame];
}
- (void)pump {
    while(atomic_load(&paused)&&!Platform_ShouldQuit()) {
        struct timespec delay={0,20000000};nanosleep(&delay,NULL);
    }
}
- (void)acceptFrame:(const uint32_t *)pixels width:(int)w height:(int)h {
    if(!pixels||w<=0||h<=0||w>4096||h>4096)return;
    @autoreleasepool {
        NSData *copy=[NSData dataWithBytes:pixels length:(NSUInteger)w*h*4];
        [frameLock lock];latestFrame=copy;latestWidth=w;latestHeight=h;frameChanged=YES;[frameLock unlock];
    }
}
- (void)displayFrame {
    [frameLock lock];BOOL changed=frameChanged;NSData *data=latestFrame;
    int w=latestWidth,h=latestHeight;frameChanged=NO;[frameLock unlock];
    if(!changed||!data)return;
    CGDataProviderRef provider=CGDataProviderCreateWithCFData((__bridge CFDataRef)data);
    CGColorSpaceRef rgb=CGColorSpaceCreateDeviceRGB();
    /* Native pixels are 0x00RRGGBB; ARM64 little-endian storage is B,G,R,0. */
    CGImageRef image=CGImageCreate(w,h,8,32,(size_t)w*4,rgb,
        kCGBitmapByteOrder32Little|kCGImageAlphaNoneSkipFirst,provider,NULL,false,kCGRenderingIntentDefault);
    if(image){
        screen.image=[UIImage imageWithCGImage:image];CGImageRelease(image);status.hidden=YES;
        if(!report[@"first_frame_received"]) {
            report[@"first_frame_seconds"]=@(NSProcessInfo.processInfo.systemUptime-launchTime);
            report[@"first_frame_received"]=@YES;report[@"state"]=@"frames_received";
            report[@"frame_width"]=@(w);report[@"frame_height"]=@(h);[self writeReport];
        }
    }
    CGColorSpaceRelease(rgb);CGDataProviderRelease(provider);
}
- (void)showFailure:(NSString *)title message:(NSString *)message {
    if(!message.length)return;
    lastError=[NSString stringWithFormat:@"%@: %@",title,message];
    status.hidden=NO;status.text=lastError;
}
- (int)startAudio:(void (*)(int16_t *,size_t))mix {
    if(!mix)return -1;if(audioEngine)return 0;
    NSError *error=nil;AVAudioSession *session=[AVAudioSession sharedInstance];
    BOOL ok=[session setCategory:AVAudioSessionCategoryPlayback mode:AVAudioSessionModeDefault options:0 error:&error];
    if(ok)ok=[session setPreferredSampleRate:44100 error:&error];
    if(ok)ok=[session setPreferredIOBufferDuration:1024.0/44100.0 error:&error];
    if(ok)ok=[session setActive:YES error:&error];
    if(!ok){[self showFailure:@"Audio" message:error.localizedDescription?:@"Audio session unavailable"];return -1;}
    AVAudioFormat *format=[[AVAudioFormat alloc] initStandardFormatWithSampleRate:44100 channels:2];
    audioEngine=[AVAudioEngine new];
    audioSource=[[AVAudioSourceNode alloc] initWithFormat:format renderBlock:
        ^OSStatus(BOOL *silence,const AudioTimeStamp *timestamp,AVAudioFrameCount frames,AudioBufferList *out) {
            (void)timestamp;*silence=NO;
            if(out->mNumberBuffers!=2)return kAudio_ParamError;
            for(int channel=0;channel<2;channel++)
                if(!out->mBuffers[channel].mData||out->mBuffers[channel].mDataByteSize<(size_t)frames*sizeof(float))return kAudio_ParamError;
            int16_t pcm[512*2];
            for(AVAudioFrameCount at=0;at<frames;) {
                size_t count=MIN((size_t)512,(size_t)frames-at);mix(pcm,count);
                for(size_t i=0;i<count;i++)for(int channel=0;channel<2;channel++)
                    ((float *)out->mBuffers[channel].mData)[at+i]=pcm[i*2+channel]/32768.0f;
                at+=(AVAudioFrameCount)count;
            }
            return noErr;
        }];
    [audioEngine attachNode:audioSource];
    [audioEngine connect:audioSource to:audioEngine.mainMixerNode format:format];
    [audioEngine prepare];
    if(![audioEngine startAndReturnError:&error]) {
        [audioEngine stop];audioSource=nil;audioEngine=nil;
        [self showFailure:@"Audio" message:error.localizedDescription?:@"Audio engine unavailable"];return -1;
    }
    report[@"audio_started"]=@YES;[self writeReport];return 0;
}
- (void)chooseDisc {
    if(started||importing)return;
    UIDocumentPickerViewController *picker=[[UIDocumentPickerViewController alloc] initForOpeningContentTypes:@[UTTypeData] asCopy:YES];
    picker.delegate=self;picker.allowsMultipleSelection=NO;[self presentViewController:picker animated:YES completion:nil];
}
- (void)documentPicker:(UIDocumentPickerViewController *)picker didPickDocumentsAtURLs:(NSArray<NSURL *> *)urls {
    (void)picker;if(!urls.count||started||importing)return;
    NSURL *url=urls.firstObject;
    if(![url.pathExtension.lowercaseString isEqualToString:@"bin"]) {
        [self showFailure:@"BIN" message:@"Choisis le fichier .bin USA. / Choose the USA .bin file."];return;
    }
    importing=YES;choose.enabled=NO;launch.enabled=NO;status.hidden=NO;
    status.text=@"Copie du BIN… / Copying BIN…";
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED,0),^{@autoreleasepool {
        BOOL access=[url startAccessingSecurityScopedResource];
        NSFileManager *fm=NSFileManager.defaultManager;NSString *pending=[self->discPath stringByAppendingString:@".pending"];
        [fm removeItemAtPath:pending error:nil];__block NSError *copyError=nil;NSError *coordError=nil;
        NSFileCoordinator *coordinator=[[NSFileCoordinator alloc] initWithFilePresenter:nil];
        [coordinator coordinateReadingItemAtURL:url options:0 error:&coordError byAccessor:^(NSURL *readable) {
            [fm copyItemAtURL:readable toURL:[NSURL fileURLWithPath:pending] error:&copyError];
        }];
        NSError *error=coordError?:copyError;
        if(!error) {
            NSURL *target=[NSURL fileURLWithPath:self->discPath];
            if([fm fileExistsAtPath:self->discPath])
                [fm replaceItemAtURL:target withItemAtURL:[NSURL fileURLWithPath:pending] backupItemName:nil options:0 resultingItemURL:nil error:&error];
            else [fm moveItemAtPath:pending toPath:self->discPath error:&error];
        }
        if(access)[url stopAccessingSecurityScopedResource];
        dispatch_async(dispatch_get_main_queue(),^{
            self->importing=NO;self->choose.enabled=YES;self->launch.enabled=YES;
            if(error)[self showFailure:@"Import" message:error.localizedDescription];
            else self->status.text=@"BIN copié. Appuie sur Lancer. / BIN copied. Press Play.";
        });
    }});
}
- (void)writeReport {
    report[@"last_error"]=lastError?:@"";
    NSData *data=[NSJSONSerialization dataWithJSONObject:report options:NSJSONWritingPrettyPrinted error:nil];
    [data writeToFile:[userRoot stringByAppendingPathComponent:@"native-report.json"] atomically:YES];
}
- (void)startGame {
    if(started||importing)return;
    if(![NSFileManager.defaultManager fileExistsAtPath:discPath]){[self chooseDisc];return;}
    launchTime=NSProcessInfo.processInfo.systemUptime;
    started=YES;launch.enabled=NO;choose.enabled=NO;lastError=nil;
    for(UIButton *b in controls)b.enabled=YES;
    status.hidden=NO;status.text=@"Démarrage de Lekak… / Starting Lekak…";
    UIApplication.sharedApplication.idleTimerDisabled=YES;
    report[@"state"]=@"starting";report[@"device"]=UIDevice.currentDevice.model;
    report[@"ios"]=UIDevice.currentDevice.systemVersion;[self writeReport];
    LekakNativeServices services={(__bridge void *)self,openGame,presentGame,errorGame,audioGame,pumpGame};
    if(!LekakNative_Install(&services)){[self showFailure:@"Engine" message:@"Services unavailable"];return;}
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED,0),^{@autoreleasepool {
        NSString *log=[self->userRoot stringByAppendingPathComponent:@"native-log.txt"];
        freopen(log.fileSystemRepresentation,"w",stderr);setvbuf(stderr,NULL,_IOLBF,0);
        if([[[NSBundle mainBundle] objectForInfoDictionaryKey:@"LekakTestUnlockAll"] boolValue]) setenv("LEKAK_TEST_UNLOCK_ALL","1",1);
        else unsetenv("LEKAK_TEST_UNLOCK_ALL");
        int result=LekakNative_Run(self->discPath.fileSystemRepresentation,
            NSBundle.mainBundle.bundlePath.fileSystemRepresentation,self->userRoot.fileSystemRepresentation);
        dispatch_sync(dispatch_get_main_queue(),^{
            [self->audioEngine stop];self->audioSource=nil;self->audioEngine=nil;
            self->finished=YES;self->heldButtons=0;LekakNative_Clear();
            for(UIButton *b in self->controls)b.enabled=NO;
            UIApplication.sharedApplication.idleTimerDisabled=NO;
            self->report[@"state"]=@"returned";self->report[@"engine_result"]=@(result);[self writeReport];
            self->status.hidden=NO;
            self->status.text=[NSString stringWithFormat:@"%@\nCode %d. Ferme et rouvre l’application pour relancer. / Close and reopen the app to restart.",self->lastError?:@"Session terminée / Session ended",result];
        });
    }});
}
- (void)shareReport {
    if(!report[@"state"]&&[NSFileManager.defaultManager fileExistsAtPath:[userRoot stringByAppendingPathComponent:@"native-report.json"]]) {
        /* Preserve the previous launch report after an OS termination. */
    }else [self writeReport];
    NSMutableArray *items=[NSMutableArray new];
    for(NSString *name in @[@"native-report.json",@"native-log.txt"]) {
        NSString *path=[userRoot stringByAppendingPathComponent:name];
        if([NSFileManager.defaultManager fileExistsAtPath:path])[items addObject:[NSURL fileURLWithPath:path]];
    }
    if(!items.count)return;
    UIActivityViewController *vc=[[UIActivityViewController alloc] initWithActivityItems:items applicationActivities:nil];
    vc.popoverPresentationController.sourceView=share;vc.popoverPresentationController.sourceRect=share.bounds;
    [self presentViewController:vc animated:YES completion:nil];
}
- (BOOL)prefersStatusBarHidden{return YES;}
@end

@interface LekakNativeDelegate : UIResponder <UIApplicationDelegate>
@property(nonatomic,strong) UIWindow *window;
@end
@implementation LekakNativeDelegate
- (BOOL)application:(UIApplication *)app didFinishLaunchingWithOptions:(NSDictionary *)options {
    (void)app;(void)options;self.window=[[UIWindow alloc] initWithFrame:UIScreen.mainScreen.bounds];
    self.window.rootViewController=[LekakNativeController new];[self.window makeKeyAndVisible];return YES;
}
@end
int main(int argc,char **argv) {
    @autoreleasepool{return UIApplicationMain(argc,argv,nil,NSStringFromClass(LekakNativeDelegate.class));}
}
