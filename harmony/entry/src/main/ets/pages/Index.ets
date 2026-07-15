import textcalc from 'libtextcalc.so';

interface EnterInsertion {
  shouldInsert: boolean;
  text: string;
}

@Entry
@Component
struct Index {
  @State content: string = '';

  build() {
    Column() {
      Row() {
        Column() {
          Text('Text Calculator')
            .fontSize(28)
            .fontWeight(FontWeight.Bold)
            .fontColor('#182431')
          Text('Plain text / LaTeX')
            .fontSize(13)
            .fontColor('#6B7280')
            .margin({ top: 4 })
        }
        .alignItems(HorizontalAlign.Start)
        .layoutWeight(1)

        Button('Clear')
          .fontSize(14)
          .fontColor('#0A59F7')
          .backgroundColor('#E8F0FF')
          .borderRadius(18)
          .height(36)
          .padding({ left: 18, right: 18 })
          .onClick(() => {
            this.content = '';
          })
      }
      .width('100%')
      .padding({ left: 24, right: 24, top: 22, bottom: 14 })
      .alignItems(VerticalAlign.Center)

      Column() {
        TextArea({
          text: this.content,
          placeholder: '1+2*3\nsqrt(16)\n\\frac{1}{2}+\\sqrt{9}'
        })
          .width('100%')
          .height('100%')
          .fontSize(18)
          .fontFamily('monospace')
          .fontColor('#182431')
          .placeholderColor('#9AA1AA')
          .backgroundColor('#FFFFFF')
          .borderRadius(24)
          .padding(18)
          .onChange((value: string) => {
            this.handleChange(value);
          })
      }
      .width('100%')
      .layoutWeight(1)
      .padding({ left: 16, right: 16, bottom: 12 })

      Row() {
        Text('rad')
          .fontSize(12)
          .fontColor('#4B5563')
          .backgroundColor('#FFFFFF')
          .borderRadius(14)
          .padding({ left: 12, right: 12, top: 6, bottom: 6 })
        Text('k M m u n p')
          .fontSize(12)
          .fontColor('#4B5563')
          .backgroundColor('#FFFFFF')
          .borderRadius(14)
          .padding({ left: 12, right: 12, top: 6, bottom: 6 })
          .margin({ left: 8 })
        Text('LaTeX')
          .fontSize(12)
          .fontColor('#4B5563')
          .backgroundColor('#FFFFFF')
          .borderRadius(14)
          .padding({ left: 12, right: 12, top: 6, bottom: 6 })
          .margin({ left: 8 })
      }
      .width('100%')
      .padding({ left: 16, right: 16, bottom: 18 })
    }
    .width('100%')
    .height('100%')
    .backgroundColor('#F1F3F5')
  }

  private handleChange(value: string): void {
    const insertion = textcalc.buildEnterInsertion(value) as EnterInsertion;
    this.content = insertion.shouldInsert ? value + insertion.text : value;
  }
}
