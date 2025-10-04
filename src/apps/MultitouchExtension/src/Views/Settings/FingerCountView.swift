import SwiftUI

struct FingerCountView: View {
  @ObservedObject private var fingerManager = FingerManager.shared

  var body: some View {
    HStack(alignment: .top) {
      let fingerCount = fingerManager.fingerCount
      let font = Font.callout.monospaced()

      VStack(alignment: .trailing) {
        Text("total")
        Text("\(fingerCount.totalCount)").font(font)
      }
      .padding(.horizontal, 10.0)

      VStack(alignment: .trailing) {
        Text("half")
        Text("upper: \(fingerCount.upperHalfAreaCount)").font(font)
        Text("lower: \(fingerCount.lowerHalfAreaCount)").font(font)
        Text("left: \(fingerCount.leftHalfAreaCount)").font(font)
        Text("right: \(fingerCount.rightHalfAreaCount)").font(font)
      }
      .padding(.horizontal, 10.0)

      VStack(alignment: .trailing) {
        Text("quarter")
        Text("upper: \(fingerCount.upperQuarterAreaCount)").font(font)
        Text("lower: \(fingerCount.lowerQuarterAreaCount)").font(font)
        Text("left: \(fingerCount.leftQuarterAreaCount)").font(font)
        Text("right: \(fingerCount.rightQuarterAreaCount)").font(font)
      }
      .padding(.horizontal, 10.0)

      VStack(alignment: .trailing) {
        Text("palm-total")
        Text("\(fingerCount.totalPalmCount)").font(font)
      }

      VStack(alignment: .trailing) {
        Text("palm-half")
        Text("upper: \(fingerCount.upperHalfAreaPalmCount)").font(font)
        Text("lower: \(fingerCount.lowerHalfAreaPalmCount)").font(font)
        Text("left: \(fingerCount.leftHalfAreaPalmCount)").font(font)
        Text("right: \(fingerCount.rightHalfAreaPalmCount)").font(font)
      }
      .padding(.horizontal, 10.0)
    }
  }
}
